import { defineStore } from 'pinia'
import { ref } from 'vue'

export interface ToolCall {
  name: string
  args: string
  id?: string         // call_id from API (for matching tool_result)
  result?: string     // merged from tool_result message
  isError?: boolean   // true if tool_result indicates error
}

export interface ChatMessage {
  id: string
  role: 'user' | 'assistant' | 'tool'
  content: string
  timestamp: number
  tool_name?: string
  tool_calls?: ToolCall[]       // pending tool calls (from assistant messages)
  tool_call_id?: string         // link to result's call
}

export interface ChatSession {
  id: string
  title: string
  messages: ChatMessage[]
  lastMessageId?: number
  updatedAt?: string
  source?: string
}

const PAGE_SIZE = 50
const POLL_INTERVAL = 3000

/** Parse tool_calls into ToolCall[] — handles both JSON array and Python repr string */
function parseToolCalls(raw: any): ToolCall[] {
  if (!raw || raw === 'None' || raw === 'null') return []

  // Format 1: JSON array (live API)
  if (Array.isArray(raw)) {
    return raw.map((tc: any) => ({
      id: tc.id || tc.call_id || '',
      name: tc.function?.name || tc.name || 'unknown',
      args: tc.function?.arguments || tc.arguments || '{}',
    }))
  }

  const s = String(raw)
  const calls: ToolCall[] = []

  // Format 2: Python repr [{'id': 'call_00_...', 'function': {'name': 'tool', 'arguments': '...'}}]
  const blockRe = /\{[^}]*'function':\s*\{[^}]*\}[^}]*\}/g
  let bm: RegExpExecArray | null
  while ((bm = blockRe.exec(s)) !== null) {
    const block = bm[0]
    const idM = /'(?:id|call_id)':\s*'([^']+)'/.exec(block)
    const nameM = /'name':\s*'([^']+)'/.exec(block)
    const argsM = /'arguments':\s*'((?:[^'\\]|\\.)*)'/.exec(block)
    if (nameM) {
      calls.push({
        id: idM ? idM[1] : '',
        name: nameM[1],
        args: argsM ? argsM[1].replace(/\\'/g, "'") : '{}',
      })
    }
  }

  // Fallback: flat format
  if (calls.length === 0) {
    const flatRe = /'name':\s*'([^']+)'(?:[^}]*'arguments':\s*'((?:[^'\\]|\\.)*)')?/g
    let m: RegExpExecArray | null
    while ((m = flatRe.exec(s)) !== null) {
      calls.push({ name: m[1], args: (m[2] || '{}').replace(/\\'/g, "'") })
    }
  }

  // Last resort: JSON parse
  if (calls.length === 0) {
    try {
      const arr = JSON.parse(s.replace(/'/g, '"'))
      if (Array.isArray(arr)) {
        for (const tc of arr) {
          calls.push({
            name: tc.function?.name || tc.name || 'unknown',
            args: tc.function?.arguments || tc.arguments || '{}',
          })
        }
      }
    } catch { /* not JSON */ }
  }

  return calls
}

/** Merge tool_result messages into corresponding tool_call entries */
function mergeToolResults(msgs: ChatMessage[]): ChatMessage[] {
  // Build a map of call_id → assistant message index
  const pendingCalls = new Map<string, { msgIdx: number; tcIdx: number }>()
  
  for (let i = 0; i < msgs.length; i++) {
    const m = msgs[i]
    if (m.role === 'assistant' && m.tool_calls) {
      for (let j = 0; j < m.tool_calls.length; j++) {
        const tc = m.tool_calls[j]
        if (tc.id) pendingCalls.set(tc.id, { msgIdx: i, tcIdx: j })
      }
    }
    if (m.role === 'tool' && m.tool_call_id && m.tool_call_id !== 'None') {
      const target = pendingCalls.get(m.tool_call_id)
      if (target) {
        const tc = msgs[target.msgIdx].tool_calls![target.tcIdx]
        tc.result = m.content
        try {
          const obj = JSON.parse(m.content)
          tc.isError = !!(obj.error || obj.is_error || obj.success === false)
        } catch { /* not JSON */ }
        // Mark this tool_result for removal
        ;(m as any)._merged = true
      }
    }
  }
  
  // Remove merged tool_result messages
  return msgs.filter(m => !(m as any)._merged)
}

export const useChatStore = defineStore('chat', () => {
  const sessions = ref<ChatSession[]>([])
  const activeSessionId = ref<string | null>(null)
  const panelOpen = ref(false)
  const sessionsVisible = ref(true)
  const sending = ref(false)
  const messages = ref<ChatMessage[]>([])
  const loading = ref(false)
  const displayCount = ref(PAGE_SIZE)
  const hasMore = ref(false)
  let pollTimer: ReturnType<typeof setInterval> | null = null

  // ── Raw log ──────────────────────────────────────────────────
  const rawEntries = ref<any[]>([])
  const rawEnabled = ref(false)
  const MAX_RAW = 300

  function pushRaw(sessionId: string, rawMessages: any[]) {
    if (!rawEnabled.value) return
    for (const m of rawMessages) {
      rawEntries.value.push(m)
    }
    if (rawEntries.value.length > MAX_RAW) {
      rawEntries.value = rawEntries.value.slice(-MAX_RAW)
    }
  }
  function clearRaw() { rawEntries.value = [] }
  function toggleRaw() { rawEnabled.value = !rawEnabled.value }

  // ── Load sessions ───────────────────────────────────────────
  async function loadSessions() {
    loading.value = true
    try {
      const resp = await fetch('/api/hermes/sessions')
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
      const data = await resp.json()
      const raw: any[] = Array.isArray(data) ? data : (data.data || data.sessions || [])
      sessions.value = raw
        .filter((s: any) => {
          const sid = s.session_id || s.id
          if (!sid) return false
          if ((s.message_count || 0) === 0) return false
          return true
        })
        .map((s: any) => ({
          id: s.session_id || s.id,
          title: s.title || s.session_id || 'Untitled',
          lastMessageId: s.last_message_id || 0,
          updatedAt: s.updated_at || '',
          source: s.source || '',
          messages: []
        }))
    } catch (e) {
      console.warn('Failed to load Hermes sessions:', e)
    } finally {
      loading.value = false
    }
  }

  // ── Load messages ───────────────────────────────────────────
  async function loadMessages(sessionId: string) {
    try {
      const resp = await fetch(`/api/hermes/sessions/${sessionId}`)
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
      const data = await resp.json()
      const raw: any[] = Array.isArray(data) ? data : (data.data || [])

      // Push raw messages to log (for RawLog tab)
      pushRaw(sessionId, raw)

      const allMsgs: ChatMessage[] = raw.map((m: any) => {
        const role = m.role || 'assistant'
        const toolName = (m.tool_name && m.tool_name !== 'None') ? String(m.tool_name) : undefined
        const toolCallId = (m.tool_call_id && m.tool_call_id !== 'None') ? String(m.tool_call_id) : undefined

        return {
          id: String(m.id || m.message_id || Math.random()),
          role,
          content: typeof m.content === 'string' ? m.content
            : (m.content?.text || JSON.stringify(m.content || '')),
          timestamp: typeof m.timestamp === 'number' ? m.timestamp * 1000
            : (m.created_at ? new Date(m.created_at).getTime() : Date.now()),
          tool_name: toolName,
          tool_call_id: toolCallId,
          tool_calls: role === 'assistant' ? parseToolCalls(m.tool_calls || '') : undefined,
        }
      })

      // Merge tool results into their tool calls (removes standalone tool_result messages)
      const merged = mergeToolResults(allMsgs)

      const s = sessions.value.find(s => s.id === sessionId)
      if (s) s.messages = merged

      return merged
    } catch (e) {
      console.warn('Failed to load messages:', e)
      return []
    }
  }

  // ── Display window ──────────────────────────────────────────
  function applyDisplayWindow(allMsgs: ChatMessage[]) {
    displayCount.value = PAGE_SIZE
    const start = Math.max(0, allMsgs.length - displayCount.value)
    messages.value = allMsgs.slice(start)
    hasMore.value = start > 0
  }

  function loadOlderMessages() {
    const sid = activeSessionId.value
    if (!sid) return
    const s = sessions.value.find(s => s.id === sid)
    if (!s) return
    const all = s.messages
    const prevCount = displayCount.value
    displayCount.value = Math.min(all.length, displayCount.value + PAGE_SIZE)
    if (displayCount.value === prevCount) return
    const start = Math.max(0, all.length - displayCount.value)
    messages.value = all.slice(start)
    hasMore.value = start > 0
  }

  // ── Open session ────────────────────────────────────────────
  async function openSession(sessionId: string) {
    activeSessionId.value = sessionId
    panelOpen.value = true
    displayCount.value = PAGE_SIZE
    const allMsgs = await loadMessages(sessionId)
    applyDisplayWindow(allMsgs)
    startPolling()
  }

  // ── Send message ────────────────────────────────────────────
  async function sendMessage(text: string) {
    if (!text.trim() || sending.value) return
    const sid = activeSessionId.value

    messages.value.push({
      id: 'local-' + Date.now(),
      role: 'user',
      content: text,
      timestamp: Date.now()
    })
    sending.value = true

    try {
      const resp = await fetch('/api/chat', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message: text, sessionId: sid || '' })
      })
      const data = await resp.json()

      if (data.sessionId && !sid) {
        activeSessionId.value = data.sessionId
      }

      loadSessions()

      const newSid = data.sessionId || sid
      if (newSid) {
        const allMsgs = await loadMessages(newSid)
        applyDisplayWindow(allMsgs)
      }
    } catch (e) {
      messages.value.push({
        id: 'err-' + Date.now(),
        role: 'assistant',
        content: 'Error: cannot reach server',
        timestamp: Date.now()
      })
    } finally {
      sending.value = false
    }
  }

  // ── Update sessions list from raw API data ──────────────────
  function updateSessionsList(raw: any[]) {
    const existing = new Map<string, ChatSession>()
    for (const s of sessions.value) existing.set(s.id, s)

    const merged: ChatSession[] = []
    for (const s of raw) {
      const id = s.session_id || s.id
      if (!id || (s.message_count || 0) === 0) continue
      const prev = existing.get(id)
      merged.push({
        id,
        title: s.title || id || 'Untitled',
        lastMessageId: s.last_message_id || prev?.lastMessageId || 0,
        updatedAt: s.updated_at || '',
        source: s.source || prev?.source || '',
        messages: prev?.messages || [],
      })
    }

    sessions.value = merged
  }

  // ── Polling ─────────────────────────────────────────────────
  async function pollForUpdates() {
    const sid = activeSessionId.value
    if (!sid || !panelOpen.value) return
    try {
      const resp = await fetch('/api/hermes/sessions')
      const data = await resp.json()
      const raw: any[] = Array.isArray(data) ? data : (data.data || [])

      updateSessionsList(raw)

      const fresh = raw.find((r: any) => (r.session_id || r.id) === sid)
      if (!fresh) return

      const serverCount = fresh.message_count || 0
      const localCount = messages.value.filter(m => !m.id.startsWith('local-')).length

      if (serverCount > localCount) {
        const allMsgs = await loadMessages(sid)

        let maxId = 0
        for (const m of messages.value) {
          const id = Number(m.id)
          if (!isNaN(id) && id > maxId) maxId = id
        }

        const newMsgs = allMsgs.filter(m => {
          const id = Number(m.id)
          return !isNaN(id) && id > maxId
        })

        if (newMsgs.length > 0) {
          const newUserMsgs = newMsgs.filter(m => m.role === 'user')
          const deduped = messages.value.filter(m => {
            if (!m.id.startsWith('local-')) return true
            const hasServerEq = newUserMsgs.some(nm =>
              Math.abs(nm.timestamp - m.timestamp) < 10000)
            return !hasServerEq
          })
          messages.value = [...deduped, ...newMsgs]

          // ── Update existing tool_calls with newly arrived results ──
          const resultsByCallId = new Map<string, { result: string; isError: boolean }>()
          for (const m of allMsgs) {
            if (m.role === 'tool' && m.tool_call_id && m.tool_call_id !== 'None') {
              let isErr = false
              try {
                const obj = JSON.parse(m.content)
                isErr = !!(obj.error || obj.is_error || obj.success === false)
              } catch { /* not JSON */ }
              resultsByCallId.set(m.tool_call_id, { result: m.content, isError: isErr })
            }
          }
          for (const m of messages.value) {
            if (m.tool_calls) {
              for (const tc of m.tool_calls) {
                if (tc.id && tc.result === undefined) {
                  const r = resultsByCallId.get(tc.id)
                  if (r) {
                    tc.result = r.result
                    tc.isError = r.isError
                  }
                }
              }
            }
          }

          displayCount.value = Math.max(PAGE_SIZE, messages.value.length)
          hasMore.value = true
        }
      }
    } catch { /* silent */ }
  }

  function startPolling() {
    stopPolling()
    pollTimer = setInterval(pollForUpdates, POLL_INTERVAL)
  }
  function stopPolling() {
    if (pollTimer) { clearInterval(pollTimer); pollTimer = null }
  }

  function newSession() {
    activeSessionId.value = null
    messages.value = []
    panelOpen.value = true
    stopPolling()
  }

  async function deleteSession(sessionId: string) {
    try { await fetch(`/api/hermes/sessions/${encodeURIComponent(sessionId)}`, { method: 'DELETE' }) }
    catch (e) { console.warn('Failed to delete session:', e) }
    sessions.value = sessions.value.filter(s => s.id !== sessionId)
    if (activeSessionId.value === sessionId) {
      const next = sessions.value[0]
      activeSessionId.value = next?.id || null
      if (next) openSession(next.id)
      else { messages.value = []; stopPolling() }
    }
  }

  function closePanel() { panelOpen.value = false; stopPolling() }
  function toggleSessions() { sessionsVisible.value = !sessionsVisible.value }

  loadSessions().then(() => {
    if (sessions.value.length > 0) {
      const telegramSessions = sessions.value.filter(s => s.source === 'telegram')
      const best = telegramSessions.length > 0
        ? telegramSessions[0]
        : sessions.value[0]

      openSession(best.id)
    }
  })

  return {
    messages, sessions, activeSessionId, panelOpen, sessionsVisible,
    sending, loading, hasMore,
    sendMessage, newSession, openSession, deleteSession,
    closePanel, toggleSessions, loadSessions, loadMessages,
    loadOlderMessages,
    rawEntries, rawEnabled, pushRaw, clearRaw, toggleRaw,
  }
})

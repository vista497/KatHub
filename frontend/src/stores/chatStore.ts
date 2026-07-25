import { defineStore } from 'pinia'
import { ref } from 'vue'

export interface ChatMessage {
  id: string
  role: 'user' | 'assistant' | 'tool'
  content: string
  timestamp: number
}

export interface ChatSession {
  id: string
  title: string
  messages: ChatMessage[]
  lastMessageId?: number
  updatedAt?: string
}

const PAGE_SIZE = 50
const POLL_INTERVAL = 3000

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

  // ── Load sessions from Hermes API ───────────────────────────
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
          const msgCount = s.message_count || 0
          // Skip junk: no session_id, 0 messages, or obviously non-dialog sessions
          if (!sid) return false
          if (msgCount === 0) return false
          return true
        })
        .map((s: any) => ({
        id: s.session_id || s.id,
        title: s.title || s.session_id || 'Untitled',
        lastMessageId: s.last_message_id || 0,
        updatedAt: s.updated_at || '',
        messages: []
      }))
    } catch (e) {
      console.warn('Failed to load Hermes sessions:', e)
    } finally {
      loading.value = false
    }
  }

  // ── Load ALL messages into cache, show only last N ──────────
  async function loadMessages(sessionId: string) {
    try {
      const resp = await fetch(`/api/hermes/sessions/${sessionId}`)
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
      const data = await resp.json()
      const raw: any[] = Array.isArray(data) ? data : (data.data || [])

      const allMsgs: ChatMessage[] = raw.map((m: any) => ({
        id: String(m.id || m.message_id || Math.random()),
        role: m.role || 'assistant',
        content: typeof m.content === 'string' ? m.content
          : (m.content?.text || JSON.stringify(m.content)),
        timestamp: typeof m.timestamp === 'number' ? m.timestamp * 1000
          : (m.created_at ? new Date(m.created_at).getTime() : Date.now())
      }))

      // Cache all messages in the session
      const s = sessions.value.find(s => s.id === sessionId)
      if (s) s.messages = allMsgs

      return allMsgs
    } catch (e) {
      console.warn('Failed to load messages:', e)
      return []
    }
  }

  // ── Set visible window (pagination) ─────────────────────────
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

  // ── Open session + start polling ────────────────────────────
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

    const msg: ChatMessage = {
      id: 'local-' + Date.now(),
      role: 'user',
      content: text,
      timestamp: Date.now()
    }
    messages.value.push(msg)
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
        loadSessions()
      }

      messages.value.push({
        id: data.message_id || data.id || 'msg-' + Date.now(),
        role: 'assistant',
        content: data.reply || data.message?.content || data.content || data.output || data.error || 'No response',
        timestamp: Date.now()
      })
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

  // ── Polling for live updates ────────────────────────────────
  async function pollForUpdates() {
    const sid = activeSessionId.value
    if (!sid || !panelOpen.value) return

    try {
      // Check sessions list for updated last_message_id
      const resp = await fetch('/api/hermes/sessions')
      const data = await resp.json()
      const raw: any[] = Array.isArray(data) ? data : (data.data || [])
      const fresh = raw.find((r: any) => (r.session_id || r.id) === sid)
      if (!fresh) return

      const serverLastId = fresh.last_message_id || 0
      const s = sessions.value.find(s => s.id === sid)
      const localLastId = s?.lastMessageId || 0

      if (serverLastId > localLastId) {
        // New messages arrived — reload
        if (s) s.lastMessageId = serverLastId
        const allMsgs = await loadMessages(sid)

        // Append new messages (not full reload — keep scroll position)
        const currentIds = new Set(messages.value.map(m => m.id))
        const newMsgs = allMsgs.filter(m => !currentIds.has(m.id))
        if (newMsgs.length > 0) {
          messages.value = [...messages.value, ...newMsgs]
          // Update displayCount to match
          const all = s?.messages || allMsgs
          const visibleCount = all.length - (allMsgs.length - messages.value.length)
          displayCount.value = Math.max(PAGE_SIZE, visibleCount)
          hasMore.value = messages.value.length < all.length
        }
      }
    } catch (e) {
      // Silent — polling failures are normal
    }
  }

  function startPolling() {
    stopPolling()
    pollTimer = setInterval(pollForUpdates, POLL_INTERVAL)
  }

  function stopPolling() {
    if (pollTimer) {
      clearInterval(pollTimer)
      pollTimer = null
    }
  }

  // ── Create new session ──────────────────────────────────────
  function newSession() {
    activeSessionId.value = null
    messages.value = []
    panelOpen.value = true
    stopPolling()
  }

  // ── Delete session ──────────────────────────────────────────
  async function deleteSession(sessionId: string) {
    try {
      await fetch(`/api/hermes/sessions/${encodeURIComponent(sessionId)}`, { method: 'DELETE' })
    } catch (e) {
      console.warn('Failed to delete session on server:', e)
    }
    sessions.value = sessions.value.filter(s => s.id !== sessionId)
    if (activeSessionId.value === sessionId) {
      const next = sessions.value[0]
      activeSessionId.value = next?.id || null
      if (next) {
        openSession(next.id)
      } else {
        messages.value = []
        stopPolling()
      }
    }
  }

  function closePanel() {
    panelOpen.value = false
    stopPolling()
  }

  function toggleSessions() { sessionsVisible.value = !sessionsVisible.value }

  // ── Initialize ──────────────────────────────────────────────
  loadSessions().then(() => {
    if (sessions.value.length > 0) {
      const sorted = [...sessions.value].sort((a, b) => (b.lastMessageId || 0) - (a.lastMessageId || 0))
      const best = sorted.find(s => (s.lastMessageId || 0) > 0) || sorted[0]
      openSession(best.id)
    }
  })

  return {
    messages, sessions, activeSessionId, panelOpen, sessionsVisible,
    sending, loading, hasMore,
    sendMessage, newSession, openSession, deleteSession,
    closePanel, toggleSessions, loadSessions, loadMessages,
    loadOlderMessages,
  }
})

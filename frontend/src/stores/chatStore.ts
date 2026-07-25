import { defineStore } from 'pinia'
import { ref } from 'vue'

export interface ChatMessage {
  id: string
  role: 'user' | 'assistant'
  content: string
  timestamp: number
}

export interface ChatSession {
  id: string
  title: string
  messages: ChatMessage[]
}

export const useChatStore = defineStore('chat', () => {
  const sessions = ref<ChatSession[]>([])
  const activeSessionId = ref<string | null>(null)
  const panelOpen = ref(false)
  const sessionsVisible = ref(true)
  const sending = ref(false)
  const messages = ref<ChatMessage[]>([])
  const loading = ref(false)

  // ── Load sessions from Hermes API ───────────────────────────
  async function loadSessions() {
    loading.value = true
    try {
      const resp = await fetch('/api/hermes/sessions')
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
      const data = await resp.json()
      // Hermes API returns {object:"list", data:[...]} or array directly
      const raw: any[] = Array.isArray(data) ? data : (data.data || data.sessions || [])
      sessions.value = raw.map((s: any) => ({
        id: s.session_id || s.id,
        title: s.title || s.session_id || 'Untitled',
        messages: []
      }))
    } catch (e) {
      console.warn('Failed to load Hermes sessions:', e)
      sessions.value = []
    } finally {
      loading.value = false
    }
  }

  // ── Load messages for a session ─────────────────────────────
  async function loadMessages(sessionId: string) {
    try {
      const resp = await fetch(`/api/hermes/sessions/${sessionId}`)
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
      const data = await resp.json()
      // Hermes API returns array of {id, role, content, created_at, ...}
      const raw: any[] = Array.isArray(data) ? data : (data.messages || [])
      // Update the session's messages cache
      const s = sessions.value.find(s => s.id === sessionId)
      if (s) {
        s.messages = raw.map((m: any) => ({
          id: m.id || m.message_id || String(Math.random()),
          role: m.role || 'assistant',
          content: typeof m.content === 'string' ? m.content
            : (m.content?.text || JSON.stringify(m.content)),
          timestamp: m.created_at ? new Date(m.created_at).getTime() : Date.now()
        }))
      }
      return s?.messages || []
    } catch (e) {
      console.warn('Failed to load messages:', e)
      return []
    }
  }

  // ── Open a session (load messages + activate) ───────────────
  async function openSession(sessionId: string) {
    activeSessionId.value = sessionId
    panelOpen.value = true
    messages.value = await loadMessages(sessionId)
  }

  // ── Send message ────────────────────────────────────────────
  async function sendMessage(text: string) {
    if (!text.trim() || sending.value) return

    // Ensure we have a session
    let sid = activeSessionId.value

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

      // If server gave us a sessionId back, use it
      if (data.sessionId && !sid) {
        sid = data.sessionId
        activeSessionId.value = sid
        // Reload sessions list (new session was created)
        loadSessions()
      }

      messages.value.push({
        id: data.message_id || data.id || 'msg-' + Date.now(),
        role: 'assistant',
        content: data.reply
          || data.message?.content
          || data.content
          || data.output
          || data.error
          || 'No response',
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

  // ── Create new session ──────────────────────────────────────
  // Opens the panel; the real session is created by Hermes on first message.
  function newSession() {
    activeSessionId.value = null
    messages.value = []
    panelOpen.value = true
  }

  // ── Delete session ──────────────────────────────────────────
  async function deleteSession(sessionId: string) {
    // Call Hermes API to delete the session
    try {
      await fetch(`/api/hermes/sessions/${encodeURIComponent(sessionId)}`, {
        method: 'DELETE'
      })
    } catch (e) {
      console.warn('Failed to delete session on server:', e)
    }
    sessions.value = sessions.value.filter(s => s.id !== sessionId)
    if (activeSessionId.value === sessionId) {
      const next = sessions.value[0]
      activeSessionId.value = next?.id || null
      if (next) {
        loadMessages(next.id).then(msgs => { messages.value = msgs })
      } else {
        messages.value = []
      }
    }
  }

  function closePanel() { panelOpen.value = false }
  function toggleSessions() { sessionsVisible.value = !sessionsVisible.value }

  // ── Initialize ──────────────────────────────────────────────
  loadSessions().then(() => {
    if (sessions.value.length > 0) {
      openSession(sessions.value[0].id)
    }
  })

  return {
    messages, sessions, activeSessionId, panelOpen, sessionsVisible,
    sending, loading,
    sendMessage, newSession, openSession, deleteSession,
    closePanel, toggleSessions, loadSessions, loadMessages,
  }
})

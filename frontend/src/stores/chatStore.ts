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

const STORAGE_KEY = 'kathub-chat-sessions'
const DEFAULT_SESSION_ID = 'telegram-default'

function loadSessions(): ChatSession[] {
  try {
    const raw = localStorage.getItem(STORAGE_KEY)
    if (raw) return JSON.parse(raw)
  } catch { /* ignore */ }
  return []
}

function saveSessions(sessions: ChatSession[]) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(sessions))
  } catch { /* quota exceeded — silently ignore */ }
}

let messageCounter = 0

export const useChatStore = defineStore('chat', () => {
  const sessions = ref<ChatSession[]>(loadSessions())
  const activeSessionId = ref<string | null>(null)
  const panelOpen = ref(false)
  const sessionsVisible = ref(true)  // toggle for sessions list
  const sending = ref(false)

  // Ensure default Telegram session exists
  function ensureDefaultSession() {
    if (sessions.value.length === 0) {
      sessions.value.push({
        id: DEFAULT_SESSION_ID,
        title: 'Telegram',
        messages: []
      })
      saveSessions(sessions.value)
    }
    // Open default if nothing active
    if (!activeSessionId.value) {
      activeSessionId.value = DEFAULT_SESSION_ID
    }
  }
  ensureDefaultSession()

  // Current session messages
  const messages = ref<ChatMessage[]>([])

  function loadActiveMessages() {
    const s = sessions.value.find(s => s.id === activeSessionId.value)
    messages.value = s ? [...s.messages] : []
  }
  loadActiveMessages()

  function persistCurrentSession() {
    const s = sessions.value.find(s => s.id === activeSessionId.value)
    if (s) {
      s.messages = [...messages.value]
      // Update title from first user message
      const firstUser = s.messages.find(m => m.role === 'user')
      if (firstUser && s.title === 'New chat') {
        s.title = firstUser.content.slice(0, 30)
      }
      saveSessions(sessions.value)
    }
  }

  async function sendMessage(text: string) {
    if (!text.trim() || sending.value) return
    if (!activeSessionId.value) {
      newSession()
    }

    const msg: ChatMessage = {
      id: String(++messageCounter),
      role: 'user',
      content: text,
      timestamp: Date.now()
    }
    messages.value.push(msg)
    persistCurrentSession()
    sending.value = true

    try {
      const resp = await fetch('/api/chat', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message: text })
      })
      const data = await resp.json()

      messages.value.push({
        id: String(++messageCounter),
        role: 'assistant',
        content: data.reply || data.error || 'No response',
        timestamp: Date.now()
      })
      persistCurrentSession()
    } catch (e) {
      messages.value.push({
        id: String(++messageCounter),
        role: 'assistant',
        content: 'Error: cannot reach server',
        timestamp: Date.now()
      })
      persistCurrentSession()
    } finally {
      sending.value = false
    }
  }

  function newSession() {
    const id = 'session-' + Date.now()
    const session: ChatSession = { id, title: 'New chat', messages: [] }
    sessions.value.unshift(session)
    saveSessions(sessions.value)
    activeSessionId.value = id
    messages.value = []
    panelOpen.value = true
  }

  function openSession(sessionId: string) {
    activeSessionId.value = sessionId
    loadActiveMessages()
    panelOpen.value = true
  }

  function deleteSession(sessionId: string) {
    sessions.value = sessions.value.filter(s => s.id !== sessionId)
    saveSessions(sessions.value)
    if (activeSessionId.value === sessionId) {
      activeSessionId.value = sessions.value[0]?.id || null
      loadActiveMessages()
    }
  }

  function closePanel() {
    panelOpen.value = false
  }

  function toggleSessions() {
    sessionsVisible.value = !sessionsVisible.value
  }

  return {
    messages, sessions, activeSessionId, panelOpen, sessionsVisible,
    sending,
    sendMessage, newSession, openSession, deleteSession,
    closePanel, toggleSessions,
    loadActiveMessages, persistCurrentSession,
  }
})

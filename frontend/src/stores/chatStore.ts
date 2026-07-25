import { defineStore } from 'pinia'
import { ref } from 'vue'

export interface ChatMessage {
  id: string
  role: 'user' | 'assistant'
  content: string
  timestamp: number
}

function loadSessions(): { id: string; title: string }[] {
  try {
    const raw = localStorage.getItem('kathub-chat-sessions')
    return raw ? JSON.parse(raw) : []
  } catch { return [] }
}

function saveSessions(sessions: { id: string; title: string }[]) {
  localStorage.setItem('kathub-chat-sessions', JSON.stringify(sessions))
}

let messageCounter = 0

export const useChatStore = defineStore('chat', () => {
  const messages = ref<ChatMessage[]>([])
  const sessions = ref<{ id: string; title: string }[]>(loadSessions())
  const activeSessionId = ref<string | null>(null)
  const panelOpen = ref(false)
  const activeChat = ref<string | null>(null)
  const sending = ref(false)
  const messagesLoading = ref(false)
  const sessionsLoading = ref(false)

  async function sendMessage(text: string) {
    if (!text.trim() || sending.value) return

    const msg: ChatMessage = {
      id: String(++messageCounter),
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
        body: JSON.stringify({ message: text })
      })
      const data = await resp.json()

      messages.value.push({
        id: String(++messageCounter),
        role: 'assistant',
        content: data.reply || data.error || 'No response',
        timestamp: Date.now()
      })
    } catch (e) {
      messages.value.push({
        id: String(++messageCounter),
        role: 'assistant',
        content: 'Error: cannot reach server',
        timestamp: Date.now()
      })
    } finally {
      sending.value = false
    }
  }

  function newSession() {
    const id = 'session-' + Date.now()
    activeSessionId.value = id
    messages.value = []
    sessions.value.unshift({ id, title: 'New chat' })
    saveSessions(sessions.value)
    panelOpen.value = true
    activeChat.value = id
  }

  function toggleChat(sessionId: string) {
    activeChat.value = sessionId
    activeSessionId.value = sessionId
    if (panelOpen.value && activeChat.value === sessionId) {
      panelOpen.value = false
    } else {
      panelOpen.value = true
    }
  }

  function fetchSessions() {
    sessions.value = loadSessions()
  }

  function openSession(id: string) {
    activeSessionId.value = id
  }

  return {
    messages, sessions, activeSessionId, panelOpen, activeChat,
    sending, messagesLoading, sessionsLoading,
    sendMessage, newSession, toggleChat, fetchSessions, openSession
  }
})

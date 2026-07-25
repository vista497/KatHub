import { defineStore } from 'pinia'
import { ref } from 'vue'

export interface Message {
  id: string
  role: 'user' | 'assistant'
  content: string
  timestamp: Date
}

export interface ChatSession {
  id: string          // filename without .md
  title: string
  date: string
  messageCount: number
  path: string        // relative path in vault
}

export const useChatStore = defineStore('chat', () => {
  // Sessions list
  const sessions = ref<ChatSession[]>([])
  const sessionsLoading = ref(false)
  const activeSessionId = ref<string | null>(null)

  // Active session messages
  const messages = ref<Message[]>([])
  const messagesLoading = ref(false)

  // Chat overlay state
  const activeChat = ref<string | null>(null)
  const panelOpen = ref(false)

  function toggleChat(agentName: string) {
    if (activeChat.value === agentName && panelOpen.value) {
      panelOpen.value = false
    } else {
      activeChat.value = agentName
      panelOpen.value = true
    }
  }

  async function fetchSessions() {
    sessionsLoading.value = true
    try {
      const res = await fetch('/api/vault/sessions')
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const data = await res.json()
      sessions.value = data.sessions || []
    } catch (e: any) {
      console.error('Sessions load error:', e)
    } finally {
      sessionsLoading.value = false
    }
  }

  async function openSession(sessionId: string) {
    activeSessionId.value = sessionId
    messagesLoading.value = true
    try {
      const res = await fetch(`/api/vault/sessions/${encodeURIComponent(sessionId)}`)
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const data = await res.json()
      messages.value = data.messages || []
    } catch (e: any) {
      console.error('Session load error:', e)
      messages.value = []
    } finally {
      messagesLoading.value = false
    }
  }

  async function sendMessage(text: string) {
    if (!text.trim()) return
    // Add user message locally
    const userMsg: Message = {
      id: `user-${Date.now()}`,
      role: 'user',
      content: text,
      timestamp: new Date(),
    }
    messages.value.push(userMsg)

    // Send to server
    try {
      const res = await fetch('/api/chat', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message: text }),
      })
      if (res.ok) {
        const data = await res.json()
        messages.value.push({
          id: `assistant-${Date.now()}`,
          role: 'assistant',
          content: data.response || data.message || '',
          timestamp: new Date(),
        })
      }
    } catch (e: any) {
      console.error('Chat send error:', e)
    }
  }

  return {
    sessions, sessionsLoading, activeSessionId,
    messages, messagesLoading,
    activeChat, panelOpen,
    fetchSessions, openSession, sendMessage, toggleChat,
  }
})

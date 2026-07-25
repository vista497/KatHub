import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

export interface Message {
  id: string
  role: 'user' | 'assistant'
  content: string
  timestamp: Date
}

interface ChatSession {
  id: string
  agent: string
  messages: Message[]
  unread: number
}

export const useChatStore = defineStore('chat', () => {
  const sessions = ref<ChatSession[]>([
    { id: 'katya', agent: 'Катя', messages: [], unread: 0 },
    { id: 'twin', agent: 'twin', messages: [], unread: 0 },
    { id: 'doctor', agent: 'doctor', messages: [], unread: 0 },
  ])
  const activeSession = ref<string | null>(null)
  const panelOpen = ref(false)

  const currentSession = computed(() =>
    sessions.value.find(s => s.id === activeSession.value) || null
  )

  function openSession(id: string) {
    activeSession.value = id
    panelOpen.value = true
    const s = sessions.value.find(s => s.id === id)
    if (s) s.unread = 0
  }

  function closePanel() {
    panelOpen.value = false
  }

  function sendMessage(content: string) {
    if (!activeSession.value) return
    const session = sessions.value.find(s => s.id === activeSession.value)
    if (!session) return
    session.messages.push({
      id: crypto.randomUUID(),
      role: 'user',
      content,
      timestamp: new Date(),
    })
  }

  function receiveMessage(agentId: string, content: string) {
    const session = sessions.value.find(s => s.id === agentId)
    if (!session) return
    session.messages.push({
      id: crypto.randomUUID(),
      role: 'assistant',
      content,
      timestamp: new Date(),
    })
    if (activeSession.value !== agentId) {
      session.unread++
    }
  }

  return {
    sessions,
    activeSession,
    panelOpen,
    currentSession,
    openSession,
    closePanel,
    sendMessage,
    receiveMessage,
  }
})

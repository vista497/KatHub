import { defineStore } from 'pinia'
import { ref } from 'vue'

export interface AgentInfo {
  name: string
  label: string
  description: string
}

export interface AgentChatMessage {
  id: string
  role: 'user' | 'assistant'
  content: string
  timestamp: number
}

export interface AgentChatState {
  messages: AgentChatMessage[]
  sessionId: string | null
  sending: boolean
}

/**
 * Отдельные чаты с каждым агентом (Analyst, Writer, Marketer, Coder,
 * Orchestrator, Катя). Каждый агент — отдельный профиль Hermes, у него
 * своя сессия (sessionId), история живёт в Hermes (vault/БД).
 */
export const useAgentChatStore = defineStore('agentChat', () => {
  const agents = ref<AgentInfo[]>([])
  const activeAgent = ref<string | null>(null)
  const modalOpen = ref(false)
  const error = ref<string | null>(null)

  // agent name -> chat state
  const chats = ref<Record<string, AgentChatState>>({})

  // ── List agents ──────────────────────────────────────────────
  async function loadAgents() {
    try {
      const resp = await fetch('/api/agent-chat')
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
      const data = await resp.json()
      agents.value = (data.agents || []).map((a: any) => ({
        name: a.name,
        label: a.label || a.name,
        description: a.description || '',
      }))
    } catch (e: any) {
      console.warn('Failed to load chat agents:', e)
      error.value = e.message || 'Failed to load agents'
    }
  }

  function ensureChat(name: string): AgentChatState {
    if (!chats.value[name]) {
      chats.value[name] = { messages: [], sessionId: null, sending: false }
    }
    return chats.value[name]
  }

  // ── Open/close modal ─────────────────────────────────────────
  function openChat(agentName: string) {
    activeAgent.value = agentName
    ensureChat(agentName)
    modalOpen.value = true
  }

  function closeChat() {
    modalOpen.value = false
    activeAgent.value = null
  }

  // ── Send message to a specific agent ─────────────────────────
  async function sendMessage(agentName: string, text: string) {
    if (!text.trim()) return
    const chat = ensureChat(agentName)
    if (chat.sending) return

    chat.messages.push({
      id: 'local-' + Date.now(),
      role: 'user',
      content: text,
      timestamp: Date.now(),
    })
    chat.sending = true
    error.value = null

    try {
      const resp = await fetch('/api/agent-chat', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          agent: agentName,
          message: text,
          sessionId: chat.sessionId || '',
        }),
      })
      const data = await resp.json()

      if (data.sessionId) {
        chat.sessionId = data.sessionId
      }

      chat.messages.push({
        id: 'agent-' + Date.now(),
        role: 'assistant',
        content: data.reply || '(пустой ответ)',
        timestamp: Date.now(),
      })
      if (data.error && data.error !== 'timeout') {
        error.value = `Agent error: ${data.error}`
      }
    } catch (e: any) {
      chat.messages.push({
        id: 'err-' + Date.now(),
        role: 'assistant',
        content: '⚠️ Не удалось связаться с агентом (сервер недоступен)',
        timestamp: Date.now(),
      })
    } finally {
      chat.sending = false
    }
  }

  function resetChat(agentName: string) {
    chats.value[agentName] = { messages: [], sessionId: null, sending: false }
  }

  loadAgents()

  return {
    agents, activeAgent, modalOpen, error, chats,
    loadAgents, openChat, closeChat, sendMessage, resetChat,
  }
})

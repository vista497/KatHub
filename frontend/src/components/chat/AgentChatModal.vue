<script setup lang="ts">
import { ref, watch, nextTick, computed } from 'vue'
import { useAgentChatStore } from '../../stores/agentChatStore'

const store = useAgentChatStore()

const draft = ref('')
const messagesEnd = ref<HTMLDivElement>()

const chat = computed(() =>
  store.activeAgent ? (store.chats[store.activeAgent] || null) : null,
)

const activeLabel = computed(() => {
  const a = store.agents.find(x => x.name === store.activeAgent)
  return a?.label || store.activeAgent || ''
})

const activeDescription = computed(() => {
  const a = store.agents.find(x => x.name === store.activeAgent)
  return a?.description || ''
})

const busy = computed(() => chat.value?.sending || false)

function send() {
  const text = draft.value.trim()
  if (!text || !store.activeAgent || busy.value) return
  draft.value = ''
  store.sendMessage(store.activeAgent, text)
}

function selectAgent(name: string) {
  store.openChat(name)
}

function onKeydown(e: KeyboardEvent) {
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault()
    send()
  }
}

watch(() => chat.value?.messages.length, async () => {
  await nextTick()
  messagesEnd.value?.scrollIntoView({ behavior: 'smooth' })
})
</script>

<template>
  <Teleport to="body">
    <div v-if="store.modalOpen" class="agent-chat-overlay" @click.self="store.closeChat()">
      <div class="agent-chat-modal">
        <!-- Header -->
        <div class="agent-chat-header">
          <div class="header-title">
            <span class="agent-badge">{{ activeLabel.charAt(0) }}</span>
            <div class="header-text">
              <span class="agent-name">{{ activeLabel }}</span>
              <span class="agent-desc">{{ activeDescription }}</span>
            </div>
          </div>
          <button class="close-btn" @click="store.closeChat()">✕</button>
        </div>

        <!-- Agent selector -->
        <div class="agent-tabs">
          <button
            v-for="a in store.agents"
            :key="a.name"
            class="agent-tab"
            :class="{ active: a.name === store.activeAgent }"
            @click="selectAgent(a.name)"
          >
            {{ a.label }}
          </button>
        </div>

        <!-- Messages -->
        <div class="agent-messages">
          <template v-if="chat && chat.messages.length > 0">
            <div
              v-for="m in chat.messages"
              :key="m.id"
              class="msg"
              :class="m.role === 'user' ? 'msg-user' : 'msg-agent'"
            >
              <div class="msg-bubble">{{ m.content }}</div>
              <div class="msg-time">{{ new Date(m.timestamp).toLocaleTimeString() }}</div>
            </div>
          </template>
          <div v-else class="empty-hint">
            Напишите первому агенту {{ activeLabel }} — это создаст отдельную сессию
            этого агента.
          </div>
          <div ref="messagesEnd"></div>
        </div>

        <!-- Typing indicator -->
        <div v-if="busy" class="typing">
          <span class="typing-dot"></span><span class="typing-dot"></span><span class="typing-dot"></span>
          <span class="typing-text">{{ activeLabel }} печатает…</span>
        </div>

        <!-- Input -->
        <div class="agent-input-row">
          <textarea
            v-model="draft"
            class="agent-input"
            rows="1"
            placeholder="Сообщение агенту… (Enter — отправить)"
            :disabled="busy"
            @keydown="onKeydown"
          ></textarea>
          <button class="send-btn" :disabled="busy || !draft.trim()" @click="send">
            ➤
          </button>
        </div>
      </div>
    </div>
  </Teleport>
</template>

<style scoped>
.agent-chat-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.55);
  backdrop-filter: blur(2px);
  z-index: 1000;
  display: flex;
  align-items: center;
  justify-content: center;
}

.agent-chat-modal {
  width: min(640px, calc(100vw - 32px));
  height: min(70vh, 620px);
  display: flex;
  flex-direction: column;
  background: var(--color-bg-primary, #1a1a2e);
  border: 1px solid var(--color-border, rgba(124, 92, 255, 0.25));
  border-radius: 14px;
  box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
  overflow: hidden;
}

/* ── Header ─────────────────────────────────── */
.agent-chat-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 14px 18px;
  background: var(--color-bg-secondary, #22223a);
  border-bottom: 1px solid var(--color-border, rgba(124, 92, 255, 0.12));
  flex-shrink: 0;
}

.header-title {
  display: flex;
  align-items: center;
  gap: 12px;
  min-width: 0;
}

.agent-badge {
  width: 38px;
  height: 38px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: 700;
  font-size: 16px;
  color: #fff;
  background: linear-gradient(135deg, #7c5cff, #4a9eff);
  flex-shrink: 0;
}

.header-text {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.agent-name {
  font-weight: 700;
  font-size: 15px;
  color: var(--color-text-primary, #eee);
}

.agent-desc {
  font-size: 12px;
  color: var(--color-text-muted, #999);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.close-btn {
  background: none;
  border: none;
  color: var(--color-text-secondary, #bbb);
  font-size: 16px;
  cursor: pointer;
  padding: 6px 10px;
  border-radius: 8px;
}

.close-btn:hover {
  background: rgba(255, 255, 255, 0.08);
  color: #fff;
}

/* ── Agent tabs ─────────────────────────────── */
.agent-tabs {
  display: flex;
  gap: 6px;
  padding: 10px 14px;
  overflow-x: auto;
  background: var(--color-bg-secondary, #22223a);
  border-bottom: 1px solid var(--color-border, rgba(124, 92, 255, 0.1));
  flex-shrink: 0;
}

.agent-tab {
  padding: 5px 12px;
  border-radius: 999px;
  border: 1px solid var(--color-border, rgba(124, 92, 255, 0.2));
  background: transparent;
  color: var(--color-text-secondary, #bbb);
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
  white-space: nowrap;
  transition: all 0.15s;
}

.agent-tab.active {
  background: rgba(124, 92, 255, 0.18);
  border-color: #7c5cff;
  color: #fff;
}

.agent-tab:hover:not(.active) {
  border-color: #7c5cff;
  color: #ddd;
}

/* ── Messages ───────────────────────────────── */
.agent-messages {
  flex: 1;
  overflow-y: auto;
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.msg {
  display: flex;
  flex-direction: column;
  max-width: 85%;
}

.msg-user {
  align-self: flex-end;
  align-items: flex-end;
}

.msg-agent {
  align-self: flex-start;
  align-items: flex-start;
}

.msg-bubble {
  padding: 10px 14px;
  border-radius: 12px;
  font-size: 14px;
  line-height: 1.5;
  white-space: pre-wrap;
  word-break: break-word;
}

.msg-user .msg-bubble {
  background: linear-gradient(135deg, #7c5cff, #5a3dd8);
  color: #fff;
  border-bottom-right-radius: 4px;
}

.msg-agent .msg-bubble {
  background: var(--color-bg-tertiary, #2a2a44);
  color: var(--color-text-primary, #eee);
  border: 1px solid var(--color-border, rgba(124, 92, 255, 0.12));
  border-bottom-left-radius: 4px;
}

.msg-time {
  font-size: 10px;
  color: var(--color-text-muted, #777);
  margin-top: 3px;
  padding: 0 4px;
}

.empty-hint {
  margin: auto;
  text-align: center;
  color: var(--color-text-muted, #888);
  font-size: 13px;
  max-width: 280px;
}

/* ── Typing indicator ───────────────────────── */
.typing {
  display: flex;
  align-items: center;
  gap: 5px;
  padding: 4px 18px 8px;
}

.typing-dot {
  width: 7px;
  height: 7px;
  border-radius: 50%;
  background: #7c5cff;
  animation: typing-bounce 1.2s infinite ease-in-out;
}

.typing-dot:nth-child(2) { animation-delay: 0.15s; }
.typing-dot:nth-child(3) { animation-delay: 0.3s; }

@keyframes typing-bounce {
  0%, 60%, 100% { transform: translateY(0); opacity: 0.5; }
  30% { transform: translateY(-4px); opacity: 1; }
}

.typing-text {
  font-size: 11px;
  color: var(--color-text-muted, #888);
  margin-left: 4px;
}

/* ── Input ──────────────────────────────────── */
.agent-input-row {
  display: flex;
  align-items: flex-end;
  gap: 8px;
  padding: 12px 14px;
  border-top: 1px solid var(--color-border, rgba(124, 92, 255, 0.12));
  background: var(--color-bg-secondary, #22223a);
  flex-shrink: 0;
}

.agent-input {
  flex: 1;
  resize: none;
  padding: 10px 12px;
  border-radius: 10px;
  border: 1px solid var(--color-border, rgba(124, 92, 255, 0.25));
  background: var(--color-bg-primary, #1a1a2e);
  color: var(--color-text-primary, #eee);
  font-size: 14px;
  font-family: inherit;
  outline: none;
  max-height: 120px;
}

.agent-input:focus {
  border-color: #7c5cff;
}

.send-btn {
  width: 42px;
  height: 42px;
  border-radius: 10px;
  border: none;
  background: linear-gradient(135deg, #7c5cff, #5a3dd8);
  color: #fff;
  font-size: 16px;
  cursor: pointer;
  flex-shrink: 0;
  transition: opacity 0.15s;
}

.send-btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}
</style>

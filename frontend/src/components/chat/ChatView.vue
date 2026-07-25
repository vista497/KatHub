<script setup lang="ts">
import { ref, watch, nextTick, computed } from 'vue'
import ChatMessage from './ChatMessage.vue'
import ChatInput from './ChatInput.vue'
import { useChatStore } from '../../stores/chatStore'

const chat = useChatStore()
const messagesEnd = ref<HTMLDivElement>()
const showSessions = ref(false)

const activeTitle = computed(() =>
  chat.sessions.find(s => s.id === chat.activeSessionId)?.title || 'Chat',
)

function handleSend(text: string) {
  chat.sendMessage(text)
}

function selectSession(id: string) {
  chat.openSession(id)
  showSessions.value = false
}

watch(() => chat.messages.length, async () => {
  await nextTick()
  messagesEnd.value?.scrollIntoView({ behavior: 'smooth' })
})
</script>

<template>
  <div class="chat-view">
    <!-- Header with session selector -->
    <div class="chat-header-bar">
      <button class="sessions-toggle" @click="showSessions = !showSessions">
        ☰
      </button>
      <span class="session-name" @click="showSessions = !showSessions">
        {{ activeTitle }}
      </span>
      <button class="new-chat-btn" @click="chat.newSession()">+</button>
    </div>

    <!-- Session list dropdown -->
    <div v-if="showSessions" class="session-dropdown">
      <div
        v-for="s in chat.sessions"
        :key="s.id"
        class="session-option"
        :class="{ active: s.id === chat.activeSessionId }"
        @click="selectSession(s.id)"
      >
        <span class="session-option-title">{{ s.title }}</span>
        <button
          class="session-option-delete"
          @click.stop="chat.deleteSession(s.id)"
        >✕</button>
      </div>
      <div v-if="chat.sessions.length === 0" class="no-sessions">
        Нет чатов
      </div>
    </div>

    <!-- Messages -->
    <div class="chat-messages" @click="showSessions = false">
      <ChatMessage v-for="m in chat.messages" :key="m.id" :message="m" />
      <div ref="messagesEnd"></div>
    </div>

    <ChatInput @send="handleSend" />
  </div>
</template>

<style scoped>
.chat-view {
  display: flex;
  flex-direction: column;
  height: 100%;
  background: var(--color-bg-primary);
}

/* ── Header ──────────────────────────────────── */
.chat-header-bar {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 12px;
  padding-top: calc(8px + env(safe-area-inset-top, 0px));
  border-bottom: 1px solid rgba(124, 92, 255, 0.12);
  background: var(--color-bg-secondary);
  flex-shrink: 0;
}

.sessions-toggle {
  background: none;
  border: none;
  color: #aaaacc;
  font-size: 18px;
  cursor: pointer;
  padding: 4px;
  line-height: 1;
}

.session-name {
  flex: 1;
  font-size: 14px;
  font-weight: 600;
  color: #ccccee;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  cursor: pointer;
}

.new-chat-btn {
  background: rgba(124, 92, 255, 0.15);
  border: 1px solid rgba(124, 92, 255, 0.2);
  border-radius: 6px;
  color: #ccccee;
  cursor: pointer;
  font-size: 16px;
  width: 32px;
  height: 32px;
  display: flex;
  align-items: center;
  justify-content: center;
}

/* ── Session dropdown ────────────────────────── */
.session-dropdown {
  position: absolute;
  top: 44px;
  left: 0;
  right: 0;
  z-index: 10;
  background: #0d0d24;
  border-bottom: 1px solid rgba(124, 92, 255, 0.15);
  max-height: 60vh;
  overflow-y: auto;
  box-shadow: 0 8px 24px rgba(0,0,0,0.5);
}

.session-option {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 14px 16px;
  border-bottom: 1px solid rgba(124, 92, 255, 0.06);
  cursor: pointer;
  transition: background 0.1s;
  min-height: 48px;
}

.session-option:hover,
.session-option:active {
  background: rgba(124, 92, 255, 0.1);
}

.session-option.active {
  background: rgba(124, 92, 255, 0.15);
}

.session-option-title {
  font-size: 13px;
  color: #aaaacc;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  flex: 1;
}

.session-option.active .session-option-title {
  color: #fff;
}

.session-option-delete {
  background: none;
  border: none;
  color: #666888;
  cursor: pointer;
  font-size: 16px;
  padding: 8px 12px;
  min-width: 44px;
  min-height: 44px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.session-option-delete:hover {
  color: #ff6b6b;
}

.no-sessions {
  padding: 16px;
  text-align: center;
  color: #666888;
  font-size: 13px;
}

/* ── Messages ────────────────────────────────── */
.chat-messages {
  flex: 1;
  overflow-y: auto;
  padding: 8px 12px;
  display: flex;
  flex-direction: column;
  gap: 4px;
}
</style>

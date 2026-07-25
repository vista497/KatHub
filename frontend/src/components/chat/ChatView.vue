<script setup lang="ts">
import ChatMessage from './ChatMessage.vue'
import ChatInput from './ChatInput.vue'
import { useChatStore } from '../../stores/chatStore'
import { computed, ref, watch, nextTick } from 'vue'

const chat = useChatStore()
const messagesContainer = ref<HTMLDivElement>()

const session = computed(() => chat.currentSession)

function scrollToBottom() {
  if (messagesContainer.value) {
    messagesContainer.value.scrollTop = messagesContainer.value.scrollHeight
  }
}

watch(() => session.value?.messages.length, async () => {
  await nextTick()
  scrollToBottom()
})

// On mobile, use first session by default
if (!chat.activeSession) {
  chat.openSession('katya')
}
</script>

<template>
  <div class="chat-view">
    <!-- Agent switcher -->
    <div class="agent-tabs">
      <button
        v-for="s in chat.sessions"
        :key="s.id"
        class="agent-tab"
        :class="{ active: chat.activeSession === s.id }"
        @click="chat.openSession(s.id)"
      >
        {{ s.agent }}
        <span v-if="s.unread" class="unread-dot"></span>
      </button>
    </div>

    <!-- Messages -->
    <div class="chat-messages" ref="messagesContainer">
      <ChatMessage
        v-for="msg in session?.messages"
        :key="msg.id"
        :message="msg"
      />
      <div v-if="!session?.messages.length" class="empty-state">
        No messages yet
      </div>
    </div>

    <!-- Input -->
    <ChatInput @send="chat.sendMessage" />
  </div>
</template>

<style scoped>
.chat-view {
  display: flex;
  flex-direction: column;
  height: 100%;
  background: var(--color-chat-bg);
}

.agent-tabs {
  display: flex;
  gap: var(--space-1);
  padding: var(--space-2);
  background: var(--color-bg-secondary);
  border-bottom: 1px solid var(--color-border);
  overflow-x: auto;
}

.agent-tab {
  padding: var(--space-1) var(--space-3);
  background: none;
  border: 1px solid var(--color-border);
  border-radius: var(--radius-full);
  color: var(--color-text-secondary);
  font-size: var(--font-size-sm);
  cursor: pointer;
  white-space: nowrap;
  transition: all var(--transition-fast);
  font-family: var(--font-sans);
  position: relative;
}

.agent-tab.active {
  background: var(--color-accent);
  color: white;
  border-color: var(--color-accent);
}

.unread-dot {
  position: absolute;
  top: 2px;
  right: 6px;
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: var(--color-accent-secondary);
}

.chat-messages {
  flex: 1;
  overflow-y: auto;
  padding: var(--space-4);
  display: flex;
  flex-direction: column;
  gap: var(--space-2);
}

.empty-state {
  margin: auto;
  color: var(--color-text-muted);
  font-size: var(--font-size-sm);
}
</style>

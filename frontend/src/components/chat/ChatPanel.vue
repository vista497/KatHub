<script setup lang="ts">
import { computed, ref, watch, nextTick } from 'vue'
import { useChatStore } from '../../stores/chatStore'
import ChatMessage from './ChatMessage.vue'
import ChatInput from './ChatInput.vue'

const emit = defineEmits<{ close: [] }>()
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
</script>

<template>
  <div class="chat-panel">
    <!-- Header -->
    <div class="chat-header">
      <span class="agent-name">{{ session?.agent || 'Chat' }}</span>
      <button class="close-btn" @click="emit('close')">✕</button>
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
.chat-panel {
  width: var(--chat-panel-width);
  height: 100vh;
  display: flex;
  flex-direction: column;
  background: var(--color-chat-bg);
  border-left: 1px solid var(--color-border);
}

.chat-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: var(--space-3) var(--space-4);
  border-bottom: 1px solid var(--color-border);
  background: var(--color-bg-secondary);
  min-height: 48px;
}

.agent-name {
  font-weight: 600;
  font-size: var(--font-size-base);
}

.close-btn {
  background: none;
  border: none;
  color: var(--color-text-secondary);
  cursor: pointer;
  font-size: var(--font-size-lg);
  padding: var(--space-1);
  border-radius: var(--radius-sm);
  transition: all var(--transition-fast);
}

.close-btn:hover {
  color: var(--color-text-primary);
  background: var(--color-surface-hover);
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

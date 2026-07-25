<script setup lang="ts">
import { watch, ref, nextTick } from 'vue'
import { useChatStore } from '../../stores/chatStore'
import ChatMessage from './ChatMessage.vue'
import ChatInput from './ChatInput.vue'

const chat = useChatStore()
const messagesEnd = ref<HTMLDivElement>()

function handleSend(text: string) {
  chat.sendMessage(text)
}

watch(() => chat.messages.length, async () => {
  await nextTick()
  messagesEnd.value?.scrollIntoView({ behavior: 'smooth' })
})
</script>

<template>
  <div class="chat-panel">
    <div class="chat-header">
      <span class="chat-title">{{ chat.activeSessionId || 'Chat' }}</span>
      <span v-if="chat.sending" class="sending">...</span>
    </div>

    <div class="chat-messages">
      <ChatMessage
        v-for="m in chat.messages"
        :key="m.id"
        :message="m"
      />
      <div v-if="chat.sending" class="typing">Thinking...</div>
      <div ref="messagesEnd"></div>
    </div>

    <ChatInput @send="handleSend" :disabled="chat.sending" />
  </div>
</template>

<style scoped>
.chat-panel {
  display: flex;
  flex-direction: column;
  height: 100%;
  background: #0e0e1a;
}

.chat-header {
  padding: 10px 12px;
  border-bottom: 1px solid rgba(124,92,255,0.12);
  display: flex;
  align-items: center;
  gap: 8px;
  flex-shrink: 0;
}

.chat-title {
  font-size: 13px;
  color: #aaaacc;
  font-weight: 600;
  flex: 1;
}

.sending {
  color: #7c5cff;
  font-size: 12px;
  animation: pulse 1s infinite;
}

@keyframes pulse {
  0%, 100% { opacity: 0.3; }
  50% { opacity: 1; }
}

.chat-messages {
  flex: 1;
  overflow-y: auto;
  padding: 8px;
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.typing {
  color: #666888;
  font-size: 11px;
  padding: 4px 8px;
}
</style>

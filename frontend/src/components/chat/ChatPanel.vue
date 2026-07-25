<script setup lang="ts">
import { computed, ref, watch, nextTick } from 'vue'
import { useChatStore } from '../../stores/chatStore'
import ChatMessage from './ChatMessage.vue'
import ChatInput from './ChatInput.vue'

const chat = useChatStore()
const messagesEnd = ref<HTMLDivElement>()

const sessionTitle = computed(() => {
  const s = chat.sessions.find(s => s.id === chat.activeSessionId)
  return s?.title || chat.activeSessionId || 'Chat'
})

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
      <span class="chat-title">{{ sessionTitle }}</span>
    </div>

    <div class="chat-messages">
      <div v-if="chat.messagesLoading" class="loading">Loading...</div>
      <ChatMessage
        v-for="m in chat.messages"
        :key="m.id"
        :message="m"
      />
      <div ref="messagesEnd"></div>
    </div>

    <ChatInput @send="handleSend" />
  </div>
</template>

<style scoped>
.chat-panel {
  display: flex;
  flex-direction: column;
  height: 100%;
  background: #111122;
}

.chat-header {
  padding: 10px 12px;
  border-bottom: 1px solid rgba(124,92,255,0.15);
  flex-shrink: 0;
}

.chat-title {
  font-size: 13px;
  color: #aaaacc;
  font-weight: 600;
}

.chat-messages {
  flex: 1;
  overflow-y: auto;
  padding: 8px;
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.loading {
  color: #666888;
  font-size: 12px;
  text-align: center;
  padding: 20px;
}
</style>

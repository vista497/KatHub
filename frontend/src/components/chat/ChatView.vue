<script setup lang="ts">
import ChatMessage from './ChatMessage.vue'
import ChatInput from './ChatInput.vue'
import { useChatStore } from '../../stores/chatStore'
import { ref, watch, nextTick } from 'vue'

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
  <div class="chat-view">
    <div class="chat-messages">
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

.chat-messages {
  flex: 1;
  overflow-y: auto;
  padding: 8px;
  display: flex;
  flex-direction: column;
  gap: 4px;
}
</style>

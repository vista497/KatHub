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
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
  background: #0e0e1a;
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

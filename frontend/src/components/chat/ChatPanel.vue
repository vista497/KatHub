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

function handleClarifyChoice(choice: string) {
  chat.sendMessage(choice)
}

function shouldShowTime(index: number): boolean {
  const msgs = chat.messages
  if (index >= msgs.length - 1) return true // last message always shows time
  const curr = msgs[index].timestamp
  const next = msgs[index + 1].timestamp
  // Show time if next message is more than 60 seconds away
  return Math.abs(next - curr) > 60_000
}

watch(() => chat.messages.length, async () => {
  await nextTick()
  messagesEnd.value?.scrollIntoView({ behavior: 'smooth' })
})
</script>

<template>
  <div class="chat-panel">
    <div class="chat-messages">
      <div v-if="chat.hasMore" class="load-older" @click="chat.loadOlderMessages()">
        ↑ Load older messages
      </div>
      <ChatMessage
        v-for="(m, i) in chat.messages"
        :key="m.id"
        :message="m"
        :showTime="shouldShowTime(i)"
        @clarify-choice="handleClarifyChoice"
      />
      <div v-if="chat.sending" class="typing">Thinking...</div>
      <div ref="messagesEnd"></div>
    </div>

    <ChatInput @send="handleSend" :sending="chat.sending" />
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

.load-older {
  text-align: center;
  padding: 10px;
  color: var(--color-accent-secondary);
  font-size: 12px;
  cursor: pointer;
  border-bottom: 1px solid rgba(124, 92, 255, 0.08);
  margin-bottom: 4px;
  transition: background 0.15s;
}

.load-older:hover {
  background: rgba(124, 92, 255, 0.06);
}

.typing {
  color: #666888;
  font-size: 11px;
  padding: 4px 8px;
}
</style>

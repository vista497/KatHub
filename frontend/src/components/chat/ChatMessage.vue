<script setup lang="ts">
defineProps<{
  message: { id: string; role: string; content: string; timestamp: Date }
}>()
</script>

<template>
  <div class="message" :class="message.role">
    <div class="bubble">
      {{ message.content }}
    </div>
    <div class="time">
      {{ new Date(message.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) }}
    </div>
  </div>
</template>

<style scoped>
.message {
  display: flex;
  flex-direction: column;
  max-width: 85%;
}

.message.user {
  align-self: flex-end;
  align-items: flex-end;
}

.message.assistant {
  align-self: flex-start;
}

.bubble {
  padding: var(--space-2) var(--space-3);
  border-radius: var(--radius-md);
  font-size: var(--font-size-sm);
  line-height: 1.5;
  word-break: break-word;
}

.user .bubble {
  background: var(--color-accent);
  color: white;
  border-bottom-right-radius: var(--space-1);
}

.assistant .bubble {
  background: var(--color-chat-bubble-assistant);
  border-bottom-left-radius: var(--space-1);
}

.time {
  font-size: var(--font-size-xs);
  color: var(--color-text-muted);
  margin-top: 2px;
  padding: 0 var(--space-1);
}
</style>

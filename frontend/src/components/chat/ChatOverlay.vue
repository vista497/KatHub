<script setup lang="ts">
import { useChatStore } from '../../stores/chatStore'
import ChatPanel from './ChatPanel.vue'

const chat = useChatStore()
</script>

<template>
  <div class="chat-overlay">
    <!-- Chat strips -->
    <div class="chat-strips">
      <div
        v-for="session in chat.sessions"
        :key="session.id"
        class="chat-strip"
        :class="{ active: chat.activeSession === session.id }"
        @click="chat.openSession(session.id)"
      >
        <span class="strip-label">{{ session.agent[0] }}</span>
        <span v-if="session.unread" class="unread-badge">{{ session.unread }}</span>
      </div>
    </div>

    <!-- Chat panel (slide-in) -->
    <Transition name="slide">
      <ChatPanel v-if="chat.panelOpen" @close="chat.closePanel" />
    </Transition>
  </div>
</template>

<style scoped>
.chat-overlay {
  display: flex;
  height: 100vh;
}

.chat-strips {
  display: flex;
  flex-direction: column;
  gap: var(--space-1);
  padding: var(--space-2);
  background: var(--color-bg-secondary);
  border-left: 1px solid var(--color-border);
}

.chat-strip {
  width: 28px;
  height: 60px;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  border-radius: var(--radius-sm);
  background: var(--color-chat-strip);
  cursor: pointer;
  transition: all var(--transition-fast);
  position: relative;
}

.chat-strip:hover {
  background: var(--color-chat-strip-hover);
  width: 34px;
}

.chat-strip.active {
  background: var(--color-accent);
}

.strip-label {
  font-size: var(--font-size-sm);
  color: var(--color-text-primary);
  font-weight: 600;
}

.unread-badge {
  position: absolute;
  top: 6px;
  right: 6px;
  min-width: 16px;
  height: 16px;
  padding: 0 4px;
  background: var(--color-accent-secondary);
  color: var(--color-bg-primary);
  border-radius: var(--radius-full);
  font-size: 10px;
  font-weight: 700;
  display: flex;
  align-items: center;
  justify-content: center;
  line-height: 1;
}

/* Slide transition */
.slide-enter-active,
.slide-leave-active {
  transition: all var(--transition-slow);
}

.slide-enter-from,
.slide-leave-to {
  transform: translateX(100%);
  opacity: 0;
}
</style>

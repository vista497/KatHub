<script setup lang="ts">
import { useChatStore } from '../../stores/chatStore'
import ChatPanel from './ChatPanel.vue'

const chat = useChatStore()
</script>

<template>
  <div class="chat-overlay">
    <!-- Collapsed strips -->
    <div
      v-for="s in chat.sessions.slice(0, 8)"
      :key="s.id"
      class="chat-strip"
      :class="{ active: chat.panelOpen && chat.activeChat === s.id }"
      @click="chat.toggleChat(s.id)"
    >
      <span class="strip-label">{{ s.title.slice(0, 2) }}</span>
    </div>

    <!-- Expanded panel -->
    <Transition name="slide">
      <div v-if="chat.panelOpen && chat.activeChat" class="chat-panel-wrapper">
        <ChatPanel />
        <button class="close-btn" @click="chat.panelOpen = false">✕</button>
      </div>
    </Transition>
  </div>
</template>

<style scoped>
.chat-overlay {
  position: relative;
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  align-items: flex-end;
  gap: 2px;
  padding: 4px 0;
}

.chat-strip {
  width: 8px;
  height: 32px;
  background: rgba(124,92,255,0.25);
  border-radius: 4px 0 0 4px;
  cursor: pointer;
  transition: all 0.15s;
  display: flex;
  align-items: center;
  justify-content: center;
  overflow: hidden;
  flex-shrink: 0;
}

.chat-strip:hover, .chat-strip.active {
  width: 28px;
  background: rgba(124,92,255,0.5);
}

.strip-label {
  font-size: 8px;
  color: #ccce;
  writing-mode: vertical-lr;
  white-space: nowrap;
}

.chat-panel-wrapper {
  position: absolute;
  right: 12px;
  top: 0;
  bottom: 0;
  width: 380px;
  z-index: 100;
  background: var(--color-bg-secondary);
  border-left: 1px solid var(--color-border);
  display: flex;
  flex-direction: column;
}

.close-btn {
  position: absolute;
  top: 8px;
  right: 8px;
  background: none;
  border: none;
  color: #888a;
  cursor: pointer;
  font-size: 14px;
  z-index: 101;
}

.close-btn:hover { color: #fff; }

.slide-enter-active, .slide-leave-active {
  transition: transform 0.2s ease;
}
.slide-enter-from, .slide-leave-to {
  transform: translateX(100%);
}
</style>

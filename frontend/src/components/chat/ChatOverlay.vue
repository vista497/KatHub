<script setup lang="ts">
import { useChatStore } from '../../stores/chatStore'
import ChatPanel from './ChatPanel.vue'

const chat = useChatStore()
</script>

<template>
  <div class="chat-overlay">
    <!-- Sessions list at top -->
    <div class="sessions-bar">
      <button class="new-chat-btn" @click="chat.newSession()">+ New Chat</button>
      <div class="sessions-list">
        <div
          v-for="s in chat.sessions"
          :key="s.id"
          class="session-chip"
          :class="{ active: chat.activeSessionId === s.id }"
          @click="chat.openSession(s.id)"
        >
          {{ s.title.slice(0, 20) }}{{ s.title.length > 20 ? '…' : '' }}
        </div>
        <div v-if="chat.sessions.length === 0" class="no-sessions">
          No chats yet
        </div>
      </div>
    </div>

    <!-- Chat panel -->
    <ChatPanel />
  </div>
</template>

<style scoped>
.chat-overlay {
  display: flex;
  flex-direction: column;
  height: 100%;
  background: #0a0a14;
  border-left: 1px solid rgba(124, 92, 255, 0.12);
}

.sessions-bar {
  padding: 8px 10px;
  border-bottom: 1px solid rgba(124, 92, 255, 0.08);
  flex-shrink: 0;
}

.new-chat-btn {
  width: 100%;
  padding: 6px;
  margin-bottom: 6px;
  background: rgba(124, 92, 255, 0.15);
  border: 1px solid rgba(124, 92, 255, 0.2);
  border-radius: 6px;
  color: #ccccee;
  cursor: pointer;
  font-size: 12px;
  transition: background 0.15s;
}
.new-chat-btn:hover {
  background: rgba(124, 92, 255, 0.3);
}

.sessions-list {
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
  max-height: 60px;
  overflow-y: auto;
}

.session-chip {
  padding: 3px 8px;
  background: rgba(124, 92, 255, 0.08);
  border: 1px solid rgba(124, 92, 255, 0.1);
  border-radius: 4px;
  font-size: 11px;
  color: #aaaacc;
  cursor: pointer;
  white-space: nowrap;
  transition: all 0.15s;
}
.session-chip:hover {
  background: rgba(124, 92, 255, 0.2);
}
.session-chip.active {
  background: rgba(124, 92, 255, 0.3);
  border-color: rgba(124, 92, 255, 0.4);
  color: #fff;
}

.no-sessions {
  font-size: 11px;
  color: #666888;
  padding: 4px 0;
}
</style>

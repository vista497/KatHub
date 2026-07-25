<script setup lang="ts">
import { useChatStore } from '../../stores/chatStore'

const chat = useChatStore()
</script>

<template>
  <div class="ai-sessions">
    <button class="new-chat-btn" @click="chat.newSession()">
      + New Chat
    </button>

    <div v-if="chat.sessions.length === 0" class="empty">
      No sessions yet. Start a new chat!
    </div>

    <div
      v-for="s in chat.sessions"
      :key="s.id"
      class="session-card"
      @click="chat.toggleChat(s.id)"
    >
      <div class="s-title">{{ s.title }}</div>
    </div>
  </div>
</template>

<style scoped>
.ai-sessions {
  display: flex;
  flex-direction: column;
  gap: 2px;
  padding: 4px 0;
}

.new-chat-btn {
  width: 100%;
  padding: 8px;
  background: rgba(124,92,255,0.15);
  border: none;
  border-radius: 6px;
  color: #ccccee;
  cursor: pointer;
  font-size: 13px;
  transition: background 0.15s;
}

.new-chat-btn:hover {
  background: rgba(124,92,255,0.3);
}

.session-card {
  padding: 8px 10px;
  border-radius: 6px;
  cursor: pointer;
  transition: background 0.15s;
}

.session-card:hover {
  background: rgba(124,92,255,0.12);
}

.s-title {
  font-size: 12px;
  color: #aaaacc;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.empty {
  color: #666888;
  font-size: 12px;
  padding: 8px 0;
  text-align: center;
}
</style>

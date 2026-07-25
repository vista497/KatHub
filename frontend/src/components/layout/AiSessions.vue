<script setup lang="ts">
import { useChatStore } from '../../stores/chatStore'

const chat = useChatStore()
</script>

<template>
  <div class="ai-sessions">
    <div v-if="chat.sessions.length === 0 && !chat.sessionsLoading" class="empty">
      No sessions yet
    </div>
    <div v-if="chat.sessionsLoading" class="loading">
      Loading...
    </div>
    <div
      v-for="s in chat.sessions"
      :key="s.id"
      class="session-item"
      :class="{ active: chat.activeSessionId === s.id }"
      @click="chat.openSession(s.id)"
    >
      <span class="session-icon">💬</span>
      <div class="session-info">
        <div class="session-title">{{ s.title }}</div>
        <div class="session-meta">{{ s.date }} · {{ s.messageCount }} msgs</div>
      </div>
    </div>
  </div>

  <button class="refresh-btn" @click="chat.fetchSessions()" :disabled="chat.sessionsLoading">
    ↻ Refresh
  </button>
</template>

<style scoped>
.ai-sessions {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.session-item {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 6px 8px;
  border-radius: 6px;
  cursor: pointer;
  transition: background 0.15s;
}

.session-item:hover {
  background: rgba(124,92,255,0.1);
}

.session-item.active {
  background: rgba(124,92,255,0.18);
}

.session-icon {
  font-size: 14px;
  flex-shrink: 0;
}

.session-info {
  min-width: 0;
}

.session-title {
  font-size: 12px;
  color: #ccccee;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.session-meta {
  font-size: 10px;
  color: #666688;
}

.empty, .loading {
  padding: 12px 8px;
  font-size: 12px;
  color: #666688;
  text-align: center;
}

.refresh-btn {
  margin-top: 8px;
  width: 100%;
  padding: 4px;
  background: transparent;
  border: 1px solid rgba(124,92,255,0.2);
  border-radius: 4px;
  color: #8888aa;
  cursor: pointer;
  font-size: 11px;
}

.refresh-btn:hover {
  background: rgba(124,92,255,0.1);
  color: #ccccee;
}
</style>

<script setup lang="ts">
import ChatMessage from './ChatMessage.vue'
import ChatInput from './ChatInput.vue'
import { useChatStore } from '../../stores/chatStore'
import { ref, watch, nextTick } from 'vue'

const chat = useChatStore()
const messagesEnd = ref<HTMLDivElement>()

// For mobile: show session list or messages
const showSessions = ref(true)

function selectAndOpen(sessionId: string) {
  chat.openSession(sessionId)
  showSessions.value = false
}

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
    <!-- Session list -->
    <div v-if="showSessions" class="sessions-list">
      <h3>AI Sessions</h3>
      <div v-if="chat.sessions.length === 0 && !chat.sessionsLoading" class="empty">
        No sessions found. Load from Obsidian vault.
      </div>
      <div
        v-for="s in chat.sessions"
        :key="s.id"
        class="session-card"
        @click="selectAndOpen(s.id)"
      >
        <div class="s-title">{{ s.title }}</div>
        <div class="s-meta">{{ s.date }} · {{ s.messageCount }} msgs</div>
      </div>
      <button class="refresh-btn" @click="chat.fetchSessions()">↻ Load sessions</button>
    </div>

    <!-- Chat view -->
    <div v-else class="chat-conversation">
      <div class="chat-nav">
        <button @click="showSessions = true">← Back</button>
        <span>{{ chat.activeSessionId }}</span>
      </div>
      <div class="chat-messages">
        <ChatMessage v-for="m in chat.messages" :key="m.id" :message="m" />
        <div ref="messagesEnd"></div>
      </div>
      <ChatInput @send="handleSend" />
    </div>
  </div>
</template>

<style scoped>
.chat-view {
  display: flex;
  flex-direction: column;
  height: 100%;
  background: var(--color-bg-primary);
}

.sessions-list {
  padding: 16px;
  overflow-y: auto;
}

.sessions-list h3 {
  color: #ccccee;
  font-size: 14px;
  margin-bottom: 12px;
}

.session-card {
  padding: 12px;
  margin-bottom: 4px;
  border-radius: 8px;
  background: rgba(124,92,255,0.06);
  cursor: pointer;
  transition: background 0.15s;
}

.session-card:hover {
  background: rgba(124,92,255,0.15);
}

.s-title {
  font-size: 13px;
  color: #ccccee;
}

.s-meta {
  font-size: 11px;
  color: #666888;
  margin-top: 2px;
}

.empty {
  color: #666888;
  font-size: 13px;
  padding: 20px 0;
}

.refresh-btn {
  margin-top: 12px;
  width: 100%;
  padding: 8px;
  background: rgba(124,92,255,0.12);
  border: none;
  border-radius: 6px;
  color: #aaaacc;
  cursor: pointer;
  font-size: 13px;
}

.chat-conversation {
  display: flex;
  flex-direction: column;
  height: 100%;
}

.chat-nav {
  display: flex;
  gap: 8px;
  align-items: center;
  padding: 8px 12px;
  border-bottom: 1px solid rgba(124,92,255,0.1);
}

.chat-nav button {
  background: none;
  border: none;
  color: #8888aa;
  cursor: pointer;
  font-size: 13px;
}

.chat-nav span {
  font-size: 12px;
  color: #aaaacc;
}

.chat-messages {
  flex: 1;
  overflow-y: auto;
  padding: 8px;
}
</style>

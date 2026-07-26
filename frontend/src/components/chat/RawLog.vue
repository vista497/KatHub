<script setup lang="ts">
import { computed, ref } from 'vue'
import { useChatStore } from '../../stores/chatStore'

const chat = useChatStore()
const expanded = ref<Set<number>>(new Set())

function toggle(idx: number) {
  const next = new Set(expanded.value)
  if (next.has(idx)) next.delete(idx)
  else next.add(idx)
  expanded.value = next
}

// Reverse — newest on top
const reversed = computed(() => [...chat.rawEntries].reverse())

function roleClass(role: string): string {
  if (role === 'tool') return 'role-tool'
  if (role === 'assistant') return 'role-assistant'
  if (role === 'user') return 'role-user'
  return 'role-other'
}

function roleEmoji(role: string): string {
  if (role === 'tool') return '🔧'
  if (role === 'assistant') return '🤖'
  if (role === 'user') return '👤'
  return '📋'
}

function shortJson(obj: any): string {
  return JSON.stringify(obj, null, 2)
}
</script>

<template>
  <div class="raw-log">
    <div class="raw-log-header">
      <span class="raw-log-title">📡 Сырой лог Hermes</span>
      <span class="raw-log-count">{{ chat.rawEntries.length }} сообщений</span>
      <button class="raw-log-toggle" @click="chat.toggleRaw()">
        {{ chat.rawEnabled ? '⏸ Пауза' : '▶ Запись' }}
      </button>
      <button class="raw-log-clear" @click="chat.clearRaw()">🗑 Очистить</button>
    </div>

    <div v-if="!chat.rawEnabled" class="raw-log-disabled">
      Запись остановлена. Нажми «▶ Запись» чтобы начать сбор.
    </div>

    <div v-else-if="reversed.length === 0" class="raw-log-empty">
      Нет данных. Отправь сообщение в чат.
    </div>

    <div v-else class="raw-log-list">
      <div
        v-for="(entry, i) in reversed"
        :key="entry.id || i"
        class="raw-entry"
      >
        <div class="raw-entry-header" @click="toggle(i)">
          <span class="raw-arrow">{{ expanded.has(i) ? '▼' : '▶' }}</span>
          <span class="raw-role" :class="roleClass(entry.role || '')">
            {{ roleEmoji(entry.role || '') }}
            {{ entry.role }}
          </span>
          <span v-if="entry.tool_name && entry.tool_name !== 'None'"
                class="raw-tool-name">{{ entry.tool_name }}</span>
          <span class="raw-id">#{{ entry.id }}</span>
        </div>
        <pre v-if="expanded.has(i)" class="raw-body">{{ shortJson(entry) }}</pre>
      </div>
    </div>
  </div>
</template>

<style scoped>
.raw-log {
  display: flex;
  flex-direction: column;
  height: 100%;
  background: #0a0a1e;
  color: #c0c0d8;
  font-size: 12px;
  font-family: 'Fira Code', 'JetBrains Mono', monospace;
}

.raw-log-header {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 8px 12px;
  background: #0d0d26;
  border-bottom: 1px solid rgba(124, 92, 255, 0.15);
  flex-shrink: 0;
}

.raw-log-title {
  font-weight: 600;
  color: #e0e0ff;
  font-size: 13px;
}

.raw-log-count {
  color: #666;
  font-size: 11px;
}

.raw-log-toggle,
.raw-log-clear {
  background: rgba(124, 92, 255, 0.1);
  border: 1px solid rgba(124, 92, 255, 0.2);
  color: #b0b0d0;
  padding: 4px 10px;
  border-radius: 4px;
  cursor: pointer;
  font-size: 11px;
  font-family: var(--font-sans);
  transition: background 0.12s;
}

.raw-log-toggle { margin-left: auto; }
.raw-log-clear { margin-left: 0; }

.raw-log-toggle:hover,
.raw-log-clear:hover {
  background: rgba(124, 92, 255, 0.2);
}

.raw-log-disabled,
.raw-log-empty {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #555;
  font-size: 13px;
  font-family: var(--font-sans);
}

.raw-log-list {
  flex: 1;
  overflow-y: auto;
  padding: 4px 0;
}

.raw-entry {
  border-bottom: 1px solid rgba(255, 255, 255, 0.03);
}

.raw-entry-header {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 5px 12px;
  cursor: pointer;
  user-select: none;
  transition: background 0.1s;
}
.raw-entry-header:hover {
  background: rgba(255, 255, 255, 0.03);
}

.raw-arrow {
  color: #555;
  font-size: 8px;
  width: 10px;
  flex-shrink: 0;
}

.raw-role {
  font-size: 11px;
  padding: 1px 5px;
  border-radius: 3px;
  font-weight: 600;
}
.role-tool    { color: #64ff96; background: rgba(100,255,150,0.08); }
.role-assistant { color: #7c5cff; background: rgba(124,92,255,0.08); }
.role-user    { color: #ff8c5c; background: rgba(255,140,92,0.08); }
.role-other   { color: #888; }

.raw-tool-name {
  color: #64ff96;
  font-size: 10px;
  font-weight: 600;
}

.raw-id {
  margin-left: auto;
  color: #444;
  font-size: 10px;
}

.raw-body {
  margin: 0;
  padding: 8px 12px 8px 28px;
  font-size: 10.5px;
  color: #8888aa;
  white-space: pre-wrap;
  word-break: break-all;
  max-height: 300px;
  overflow-y: auto;
  background: #060612;
  border-top: 1px solid rgba(255,255,255,0.02);
  line-height: 1.35;
}
</style>

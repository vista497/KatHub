<script setup lang="ts">
import { ref, onMounted, onUnmounted, computed } from 'vue'

// ── Types ────────────────────────────────────────────────────────
interface KanbanTask {
  id: string
  title: string
  status: string        // todo, ready, running, done, blocked
  assignee?: string
  parents?: string[]
  created_at?: string
  summary?: string
}

const STATUS_COLUMNS = [
  { key: 'todo', label: 'Ожидает', color: '#666' },
  { key: 'ready', label: 'Готово', color: '#f0ad4e' },
  { key: 'running', label: 'В работе', color: '#5bc0de' },
  { key: 'done', label: 'Готово', color: '#5ce082' },
  { key: 'blocked', label: 'Заблокировано', color: '#ff6b6b' },
]

// ── State ────────────────────────────────────────────────────────
const tasks = ref<KanbanTask[]>([])
const loading = ref(false)
const error = ref<string | null>(null)
let pollTimer: ReturnType<typeof setInterval> | null = null

// ── Computed ─────────────────────────────────────────────────────
function tasksByStatus(status: string) {
  return computed(() => tasks.value.filter(t => t.status === status))
}

// ── API ──────────────────────────────────────────────────────────
async function loadTasks() {
  loading.value = true
  error.value = null
  try {
    const resp = await fetch('/api/kanban')
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
    const data = await resp.json()
    tasks.value = Array.isArray(data) ? data : (data.tasks || data.data || [])
  } catch (e: any) {
    error.value = e.message || 'Failed to load tasks'
    tasks.value = []
  } finally {
    loading.value = false
  }
}

function formatTime(ts?: string): string {
  if (!ts) return ''
  try {
    const d = new Date(ts)
    return d.toLocaleString()
  } catch { return ts }
}

// ── Lifecycle ────────────────────────────────────────────────────
onMounted(() => {
  loadTasks()
  pollTimer = setInterval(loadTasks, 5000)
})

onUnmounted(() => {
  if (pollTimer) clearInterval(pollTimer)
})
</script>

<template>
  <div class="kanban-panel">
    <div class="panel-header">
      <h2 class="panel-title">📋 Kanban</h2>
      <button class="btn btn-ghost" @click="loadTasks" :disabled="loading">↻ Refresh</button>
    </div>

    <div v-if="error" class="error-banner">{{ error }}</div>

    <div v-if="loading && tasks.length === 0" class="loading-state">Загрузка...</div>

    <div v-else-if="tasks.length === 0" class="empty-state">
      <p>Нет задач в канбане.</p>
      <p class="hint">Задачи создаются через Hermes CLI: <code>hermes kanban create "Название"</code></p>
    </div>

    <div v-else class="board">
      <div
        v-for="col in STATUS_COLUMNS"
        :key="col.key"
        class="column"
      >
        <div class="column-header" :style="{ borderTopColor: col.color }">
          <span class="column-dot" :style="{ background: col.color }"></span>
          <span class="column-label">{{ col.label }}</span>
          <span class="column-count">{{ tasksByStatus(col.key).value.length }}</span>
        </div>
        <div class="column-body">
          <div
            v-for="task in tasksByStatus(col.key).value"
            :key="task.id"
            class="task-card"
          >
            <div class="task-title">{{ task.title }}</div>
            <div class="task-meta">
              <span v-if="task.assignee" class="task-assignee">{{ task.assignee }}</span>
              <span v-if="task.created_at" class="task-time">{{ formatTime(task.created_at) }}</span>
            </div>
          </div>
          <div v-if="tasksByStatus(col.key).value.length === 0" class="column-empty">
            —
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.kanban-panel {
  height: 100vh;
  display: flex;
  flex-direction: column;
  background: var(--color-bg-primary);
  color: var(--color-text-primary);
  font-family: var(--font-sans);
}

.panel-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 16px 24px;
  background: var(--color-bg-secondary);
  border-bottom: 1px solid var(--color-border);
  flex-shrink: 0;
}

.panel-title {
  font-size: 18px;
  font-weight: 700;
  margin: 0;
}

/* ── Board layout ──────────────────────────── */
.board {
  flex: 1;
  display: flex;
  gap: 0;
  overflow-x: auto;
  padding: 16px;
}

.column {
  flex: 1;
  min-width: 200px;
  max-width: 320px;
  display: flex;
  flex-direction: column;
  background: var(--color-bg-secondary);
  border-radius: 8px;
  overflow: hidden;
  margin-right: 12px;
}

.column:last-child { margin-right: 0; }

.column-header {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 12px 16px;
  border-top: 3px solid #666;
  background: rgba(255,255,255,0.02);
  flex-shrink: 0;
}

.column-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  flex-shrink: 0;
}

.column-label {
  font-size: 13px;
  font-weight: 600;
  flex: 1;
}

.column-count {
  font-size: 12px;
  color: var(--color-text-muted);
  background: rgba(255,255,255,0.05);
  padding: 2px 8px;
  border-radius: 10px;
}

.column-body {
  flex: 1;
  overflow-y: auto;
  padding: 8px;
}

.column-empty {
  text-align: center;
  color: var(--color-text-muted);
  padding: 24px;
  font-size: 13px;
}

/* ── Task cards ────────────────────────────── */
.task-card {
  background: var(--color-bg-tertiary);
  border: 1px solid var(--color-border);
  border-radius: 6px;
  padding: 10px 12px;
  margin-bottom: 8px;
  transition: border-color 0.15s;
  cursor: default;
}

.task-card:hover {
  border-color: rgba(124, 92, 255, 0.3);
}

.task-title {
  font-size: 13px;
  font-weight: 600;
  line-height: 1.4;
  margin-bottom: 6px;
  word-break: break-word;
}

.task-meta {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 8px;
}

.task-assignee {
  font-size: 11px;
  color: var(--color-accent-secondary);
  background: rgba(92, 224, 255, 0.1);
  padding: 1px 6px;
  border-radius: 4px;
}

.task-time {
  font-size: 10px;
  color: var(--color-text-muted);
}

/* ── States ────────────────────────────────── */
.loading-state {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--color-text-muted);
}

.empty-state {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 8px;
  color: var(--color-text-muted);
  padding: 48px;
}

.hint {
  font-size: 12px;
  color: var(--color-text-muted);
}

.hint code {
  background: var(--color-bg-tertiary);
  padding: 2px 6px;
  border-radius: 4px;
  font-family: var(--font-mono);
  font-size: 11px;
}

.error-banner {
  padding: 10px 16px;
  margin: 0 24px;
  background: rgba(255,107,107,0.1);
  border: 1px solid rgba(255,107,107,0.25);
  border-radius: 4px;
  color: #ff6b6b;
  font-size: 13px;
}

/* ── Buttons ───────────────────────────────── */
.btn {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  padding: 6px 14px;
  border-radius: 4px;
  border: 1px solid transparent;
  font-size: 13px;
  font-family: var(--font-sans);
  cursor: pointer;
  transition: all 0.15s;
  white-space: nowrap;
}

.btn:disabled { opacity: 0.4; cursor: not-allowed; }

.btn-ghost {
  background: transparent;
  color: var(--color-text-secondary);
  border-color: var(--color-border);
}

.btn-ghost:hover:not(:disabled) {
  border-color: var(--color-accent);
  color: var(--color-accent);
}
</style>

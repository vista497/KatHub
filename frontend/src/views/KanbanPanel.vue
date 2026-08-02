<script setup lang="ts">
import { ref, onMounted, onUnmounted, computed } from 'vue'

// ── Types ────────────────────────────────────────────────────────
interface KanbanTask {
  id: string
  title: string
  status: string        // todo, ready, running, done, blocked, ...
  assignee?: string
  created_at?: string
  summary?: string
}

const STATUS_COLUMNS = [
  { key: 'todo', label: 'Ожидает', color: 'var(--text-muted)' },
  { key: 'ready', label: 'Готово к работе', color: 'var(--brand-amber)' },
  { key: 'running', label: 'В работе', color: 'var(--brand-cyan)' },
  { key: 'done', label: 'Выполнено', color: 'var(--brand-mint)' },
  { key: 'blocked', label: 'Заблокировано', color: 'var(--brand-red)' },
]

// ── State ────────────────────────────────────────────────────────
const tasks = ref<KanbanTask[]>([])
const loading = ref(false)
const error = ref<string | null>(null)
const draggingId = ref<string | null>(null)
const dragOverCol = ref<string | null>(null)
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

// ── Drag & drop: перемещение карточки между колонками ─────────────
function onDragStart(task: KanbanTask) {
  draggingId.value = task.id
}

function onDragEnd() {
  draggingId.value = null
  dragOverCol.value = null
}

function onDragOver(colKey: string) {
  dragOverCol.value = colKey
}

async function onDrop(colKey: string) {
  const taskId = draggingId.value
  draggingId.value = null
  dragOverCol.value = null
  if (!taskId) return
  const task = tasks.value.find(t => t.id === taskId)
  if (!task || task.status === colKey) return

  // Оптимистично двигаем карточку, затем подтверждаем с сервера
  const prevStatus = task.status
  task.status = colKey
  try {
    const resp = await fetch(`/api/kanban/${taskId}/status`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ status: colKey }),
    })
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
    const data = await resp.json()
    if (!data.ok) throw new Error(data.error || 'move failed')
  } catch (e: any) {
    task.status = prevStatus
    error.value = e.message || 'Не удалось перенести задачу'
  } finally {
    loadTasks()
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
      <div class="panel-header-left">
        <span class="ops-eyebrow">Tasks</span>
        <span class="ops-title">Канбан</span>
      </div>
      <button class="btn btn-ghost" @click="loadTasks" :disabled="loading">↻ Обновить</button>
    </div>

    <div v-if="error" class="error-banner">{{ error }}</div>

    <div v-if="loading && tasks.length === 0" class="loading-state">Загрузка…</div>

    <div v-else-if="tasks.length === 0" class="empty-state">
      <p>Нет задач в канбане.</p>
      <p class="hint">Задачи создаются через Hermes CLI: <code>hermes kanban create "Название"</code></p>
    </div>

    <div v-else class="board">
      <div
        v-for="col in STATUS_COLUMNS"
        :key="col.key"
        class="column"
        :class="{ 'drag-over': dragOverCol === col.key }"
        @dragover.prevent="onDragOver(col.key)"
        @dragleave="dragOverCol === col.key && (dragOverCol = null)"
        @drop.prevent="onDrop(col.key)"
      >
        <div class="column-header">
          <span class="column-dot" :style="{ background: col.color }"></span>
          <span class="column-label">{{ col.label }}</span>
          <span class="column-count">{{ tasksByStatus(col.key).value.length }}</span>
        </div>
        <div class="column-body">
          <div
            v-for="task in tasksByStatus(col.key).value"
            :key="task.id"
            class="task-card"
            :class="{ dragging: draggingId === task.id }"
            draggable="true"
            @dragstart="onDragStart(task)"
            @dragend="onDragEnd"
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
  height: 100%;
  display: flex;
  flex-direction: column;
  color: var(--text-primary);
  font-family: var(--font-sans);
  padding: var(--space-4);
  gap: var(--space-3);
}

.panel-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-shrink: 0;
}

.panel-header-left {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.ops-eyebrow {
  font: 500 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted); letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
}

.ops-title {
  font-family: var(--font-display);
  font-size: clamp(22px, 3vw, 30px);
  font-weight: 500; color: var(--text-primary);
  line-height: 1; letter-spacing: -0.02em;
}

/* ── Board layout ──────────────────────────── */
.board {
  flex: 1;
  display: flex;
  gap: var(--space-3);
  overflow-x: auto;
  padding-bottom: var(--space-2);
}

.column {
  flex: 1;
  min-width: 220px;
  max-width: 340px;
  display: flex;
  flex-direction: column;
  background: var(--bg-glass);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-lg);
  overflow: hidden;
  transition: border-color var(--transition-fast), background var(--transition-fast);
}

.column.drag-over {
  border-color: var(--brand-violet);
  background: rgba(139, 92, 246, 0.08);
}

.column-header {
  display: flex;
  align-items: center;
  gap: var(--space-2);
  padding: var(--space-3) var(--space-4);
  background: rgba(255, 255, 255, 0.02);
  border-bottom: 1px solid var(--border-glass);
  flex-shrink: 0;
}

.column-dot {
  width: 8px;
  height: 8px;
  border-radius: var(--radius-full);
  flex-shrink: 0;
  box-shadow: 0 0 6px currentColor;
}

.column-label {
  font-family: var(--font-display);
  font-size: 12px;
  font-weight: 600;
  flex: 1;
  color: var(--text-primary);
  text-transform: uppercase;
  letter-spacing: var(--letter-spacing-wide);
}

.column-count {
  font-family: var(--font-mono);
  font-size: 10px;
  color: var(--text-muted);
  background: rgba(255, 255, 255, 0.06);
  padding: 2px var(--space-2);
  border-radius: var(--radius-full);
}

.column-body {
  flex: 1;
  overflow-y: auto;
  padding: var(--space-2);
  min-height: 80px;
}

.column-empty {
  text-align: center;
  color: var(--text-muted);
  padding: var(--space-6);
  font-size: 13px;
  opacity: 0.4;
}

/* ── Task cards ────────────────────────────── */
.task-card {
  background: var(--bg-glass-solid);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-md);
  padding: var(--space-3);
  margin-bottom: var(--space-2);
  transition: border-color var(--transition-fast), opacity var(--transition-fast), transform var(--transition-fast);
  cursor: grab;
}

.task-card:hover {
  border-color: rgba(139, 92, 246, 0.4);
}

.task-card:active {
  cursor: grabbing;
}

.task-card.dragging {
  opacity: 0.45;
  transform: scale(0.97);
  border-color: var(--brand-violet);
}

.task-title {
  font-family: var(--font-display);
  font-size: 13px;
  font-weight: 500;
  line-height: 1.4;
  margin-bottom: var(--space-2);
  word-break: break-word;
  color: var(--text-primary);
}

.task-meta {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: var(--space-2);
}

.task-assignee {
  font-family: var(--font-mono);
  font-size: 9px;
  color: var(--brand-cyan);
  background: rgba(125, 211, 252, 0.1);
  padding: 1px var(--space-2);
  border-radius: var(--radius-full);
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
}

.task-time {
  font-family: var(--font-mono);
  font-size: 9px;
  color: var(--text-muted);
}

/* ── States ────────────────────────────────── */
.loading-state {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--text-muted);
  font-family: var(--font-mono);
  font-size: var(--font-size-xs);
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
  opacity: 0.5;
}

.empty-state {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: var(--space-2);
  color: var(--text-muted);
  padding: var(--space-8);
  font-family: var(--font-mono);
  font-size: var(--font-size-xs);
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
}

.hint {
  font-size: 12px;
  color: var(--text-muted);
  text-transform: none;
  letter-spacing: normal;
}

.hint code {
  background: var(--bg-glass-solid);
  padding: 2px 6px;
  border-radius: var(--radius-sm);
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--brand-cyan);
}

.error-banner {
  padding: var(--space-2) var(--space-4);
  background: rgba(242, 109, 109, 0.1);
  border: 1px solid rgba(242, 109, 109, 0.25);
  border-radius: var(--radius-md);
  color: var(--brand-red);
  font-size: 13px;
}

/* ── Buttons ───────────────────────────────── */
.btn {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  padding: 6px 14px;
  border-radius: var(--radius-full);
  border: 1px solid var(--border-glass);
  font-size: var(--font-size-xs);
  font-family: var(--font-mono);
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
  cursor: pointer;
  transition: all var(--transition-fast);
  white-space: nowrap;
}

.btn:disabled { opacity: 0.4; cursor: not-allowed; }

.btn-ghost {
  background: transparent;
  color: var(--text-muted);
  border-color: var(--border-glass);
}

.btn-ghost:hover:not(:disabled) {
  border-color: var(--brand-violet);
  color: var(--brand-violet-glow);
}
</style>

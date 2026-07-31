<script setup lang="ts">
import { ref, onMounted, computed } from 'vue'

interface AgentInfo {
  name: string
  status: string
  model: string | null
  active: boolean
}
interface TaskInfo {
  id: string
  title: string
  status: string
  assignee: string
}

const agents = ref<AgentInfo[]>([])
const tasks = ref<TaskInfo[]>([])
const loading = ref(false)
const error = ref<string | null>(null)
const logFilter = ref('all')

const AGENT_RU: Record<string, string> = {
  default: 'Катя',
  orchestrator: 'Оркестратор',
  analyst: 'Аналитик',
  writer: 'Писатель',
  marketer: 'Маркетолог',
  coder: 'Кодер',
}
const AGENT_ROLE: Record<string, string> = {
  default: 'Основной агент · диалог и интеграция',
  orchestrator: 'Координатор команды',
  analyst: 'Исследования и отчёты',
  writer: 'Тексты и контент',
  marketer: 'Продвижение',
  coder: 'Техническая реализация',
}
const AGENT_COLORS: Record<string, string> = {
  default: 'var(--brand-violet)',
  orchestrator: 'var(--brand-cyan)',
  analyst: 'var(--brand-mint)',
  writer: 'var(--brand-pink)',
  marketer: 'var(--brand-amber)',
  coder: 'var(--brand-magenta)',
}
function agentRu(name: string): string { return AGENT_RU[name] || name }
function agentRole(name: string): string { return AGENT_ROLE[name] || 'Агент Hermes' }
function agentColor(name: string): string { return AGENT_COLORS[name] || 'var(--brand-cyan)' }

const activeCount = computed(() => agents.value.filter(a => a.active).length)
const idleCount = computed(() => agents.value.filter(a => !a.active).length)
const dormantCount = computed(() => agents.value.filter(a => a.status === 'stopped' || a.status === 'dormant').length)

const agentTaskCount = (name: string) => tasks.value.filter(t => t.assignee === name && t.status !== 'done').length
const agentDoneCount = (name: string) => tasks.value.filter(t => t.assignee === name && t.status === 'done').length

const STATUS_RU: Record<string, string> = {
  todo: 'todo', in_progress: 'in progress', done: 'done',
  blocked: 'blocked', review: 'review',
}

const crewLog = computed(() => {
  const rows = tasks.value.map(t => ({
    agent: t.assignee || '—',
    task: t.title,
    status: t.status,
  }))
  if (logFilter.value === 'all') return rows
  return rows.filter(r => r.status === logFilter.value || r.agent === logFilter.value)
})

const FILTERS = ['all', 'todo', 'in_progress', 'done', 'blocked']

async function loadData() {
  loading.value = true
  error.value = null
  try {
    const [agentsResp, kanbanResp] = await Promise.all([
      fetch('/api/agents').then(r => r.ok ? r.json() : null),
      fetch('/api/kanban').then(r => r.ok ? r.json() : null),
    ])
    if (agentsResp) {
      const raw: any[] = Array.isArray(agentsResp) ? agentsResp : (agentsResp.profiles || agentsResp.data || [])
      agents.value = raw.map((a: any) => ({
        name: a.name || a.id || String(a),
        status: a.status || 'unknown',
        model: a.model || a.current_model || null,
        active: a.active !== false && (a.status === 'running' || a.status === 'active'),
      }))
    }
    if (kanbanResp) {
      const raw: any[] = Array.isArray(kanbanResp) ? kanbanResp : (kanbanResp.tasks || kanbanResp.data || [])
      tasks.value = raw.map((t: any) => ({
        id: t.id || t.task_id || String(t),
        title: t.title || t.description || '—',
        status: t.status || 'todo',
        assignee: t.assignee || '—',
      }))
    }
  } catch (e: any) {
    error.value = e.message || 'Не удалось загрузить данные'
  } finally {
    loading.value = false
  }
}

onMounted(loadData)
</script>

<template>
  <div class="crew">
    <!-- Header -->
    <div class="crew-header">
      <div class="crew-header-left">
        <span class="crew-eyebrow">Subagents</span>
        <h1 class="crew-heading">Команда.</h1>
      </div>
      <div class="crew-status-card">
        <div class="crew-status-cell">
          <span class="crew-status-dot" style="background: var(--status-active); box-shadow: 0 0 8px var(--status-active);"></span>
          <span class="crew-status-num" style="color: var(--status-active);">{{ activeCount }}</span>
          <span class="crew-status-label">Active</span>
        </div>
        <div class="crew-status-cell">
          <span class="crew-status-dot" style="background: var(--status-idle);"></span>
          <span class="crew-status-num" style="color: var(--status-idle);">{{ idleCount }}</span>
          <span class="crew-status-label">Idle</span>
        </div>
        <div class="crew-status-cell">
          <span class="crew-status-dot" style="background: var(--status-dormant);"></span>
          <span class="crew-status-num" style="color: var(--status-dormant);">{{ dormantCount }}</span>
          <span class="crew-status-label">Dormant</span>
        </div>
      </div>
    </div>

    <!-- Agent cards -->
    <div class="crew-cards">
      <div v-for="a in agents" :key="a.name" class="agent-card">
        <div class="agent-card-topbar" :style="{ background: agentColor(a.name) }"></div>
        <div class="agent-card-header">
          <span class="agent-platform-pill" :style="{ background: agentColor(a.name) + '22', color: agentColor(a.name) }">hermes</span>
          <span class="agent-status-dot" :style="a.active
            ? { background: 'var(--status-active)', boxShadow: '0 0 6px var(--status-active)' }
            : { background: 'var(--status-dormant)' }"></span>
        </div>
        <div class="agent-name">{{ agentRu(a.name) }}</div>
        <div class="agent-role">{{ agentRole(a.name) }}</div>
        <div class="agent-stats-row">
          <div class="agent-stat">
            <span class="agent-stat-label">задач</span>
            <span class="agent-stat-val">{{ agentTaskCount(a.name) }}</span>
          </div>
          <div class="agent-stat">
            <span class="agent-stat-label">сделано</span>
            <span class="agent-stat-val">{{ agentDoneCount(a.name) }}</span>
          </div>
          <div class="agent-stat">
            <span class="agent-stat-label">статус</span>
            <span class="agent-stat-val" style="font-size: 11px; text-transform: capitalize;">{{ a.status }}</span>
          </div>
        </div>
        <div class="agent-load-bar" :style="{ width: (a.active ? 82 : 12) + '%', background: agentColor(a.name) }"></div>
        <div class="agent-last-task">
          <span class="agent-last-desc">{{ a.model || '—' }}</span>
          <span class="agent-last-time">{{ a.active ? 'активен сейчас' : 'ожидает' }}</span>
        </div>
      </div>
      <div v-if="agents.length === 0 && !loading" class="empty">Нет данных об агентах</div>
    </div>

    <!-- Crew log -->
    <div class="crew-log">
      <div class="ops-card-header">
        <span class="ops-eyebrow">Crew Log</span>
        <span class="ops-title">Задачи экипажа</span>
      </div>
      <div class="crew-log-filters">
        <button
          v-for="f in FILTERS"
          :key="f"
          class="crew-filter-pill"
          :class="{ active: logFilter === f }"
          @click="logFilter = f"
        >{{ f }}</button>
      </div>
      <div class="crew-log-table-wrap">
        <table class="crew-log-table">
          <thead>
            <tr>
              <th>Agent</th>
              <th>Task</th>
              <th>Status</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(row, i) in crewLog" :key="i">
              <td>
                <span class="log-agent" :style="{ color: agentColor(row.agent) }">{{ row.agent }}</span>
              </td>
              <td class="task-col">{{ row.task }}</td>
              <td>
                <span class="log-status" :class="row.status">{{ STATUS_RU[row.status] || row.status }}</span>
              </td>
            </tr>
            <tr v-if="crewLog.length === 0 && !loading">
              <td colspan="3" class="log-empty">Нет задач</td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <div v-if="loading" class="empty">Загрузка…</div>
    <div v-else-if="error" class="empty error-text">{{ error }}</div>
  </div>
</template>

<style scoped>
.crew {
  height: 100%;
  overflow-y: auto;
  padding: var(--space-4);
  display: flex;
  flex-direction: column;
  gap: var(--space-4);
}

/* Header */
.crew-header { display: flex; align-items: center; justify-content: space-between; gap: var(--space-4); flex-wrap: wrap; }
.crew-header-left { display: flex; flex-direction: column; gap: 2px; }
.crew-eyebrow {
  font: 500 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted); letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
}
.crew-heading {
  font-family: var(--font-display);
  font-size: clamp(32px, 4vw, 52px);
  font-weight: 500; color: var(--text-primary);
  line-height: 1; letter-spacing: -0.02em;
  margin: 0;
}
.crew-status-card {
  display: flex; gap: var(--space-4);
  background: var(--bg-glass); backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass); border-radius: var(--radius-lg);
  padding: var(--space-3) var(--space-4);
}
.crew-status-cell { display: flex; flex-direction: column; align-items: center; gap: 2px; min-width: 70px; }
.crew-status-dot { width: 10px; height: 10px; border-radius: var(--radius-full); margin-bottom: 2px; }
.crew-status-num { font: 700 var(--font-size-xl) var(--font-display); line-height: 1; }
.crew-status-label {
  font: 400 9px var(--font-mono); color: var(--text-muted);
  letter-spacing: var(--letter-spacing-wide); text-transform: uppercase;
}

/* Agent cards */
.crew-cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(210px, 1fr));
  gap: var(--space-3);
}
.agent-card {
  background: var(--bg-glass); backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass); border-radius: var(--radius-lg);
  padding: var(--space-4); position: relative; overflow: hidden;
  display: flex; flex-direction: column; gap: var(--space-2);
}
.agent-card-topbar { position: absolute; top: 0; left: 0; right: 0; height: 2px; }
.agent-card-header { display: flex; align-items: center; justify-content: space-between; }
.agent-platform-pill {
  font: 500 8px var(--font-mono); padding: 1px var(--space-2);
  border-radius: var(--radius-full); letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
}
.agent-status-dot { width: 6px; height: 6px; border-radius: var(--radius-full); flex-shrink: 0; }
.agent-name { font: 500 22px var(--font-display); color: var(--text-primary); line-height: 1; letter-spacing: -0.01em; }
.agent-role { font: 400 var(--font-size-sm) var(--font-display); color: var(--text-muted); line-height: 1.4; min-height: 2.8em; }
.agent-stats-row {
  display: grid; grid-template-columns: 1fr 1fr 1fr; gap: var(--space-1);
  padding-top: var(--space-2); border-top: 1px solid var(--border-glass);
}
.agent-stat { display: flex; flex-direction: column; gap: 1px; }
.agent-stat-label {
  font: 400 8px var(--font-mono); color: var(--text-muted);
  letter-spacing: var(--letter-spacing-wide); text-transform: uppercase;
}
.agent-stat-val { font: 700 var(--font-size-base) var(--font-display); line-height: 1; color: var(--text-primary); }
.agent-load-bar { height: 3px; border-radius: var(--radius-full); margin-top: auto; opacity: 0.8; transition: width 0.5s ease; }
.agent-last-task { display: flex; flex-direction: column; gap: 1px; }
.agent-last-desc {
  font: 400 var(--font-size-xs) var(--font-mono); color: var(--text-muted);
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
}
.agent-last-time { font: 400 9px var(--font-mono); color: var(--text-muted); opacity: 0.6; }

/* Crew log */
.crew-log {
  background: var(--bg-glass); backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass); border-radius: var(--radius-lg);
  padding: var(--space-4); display: flex; flex-direction: column; gap: var(--space-3);
}
.ops-card-header { display: flex; flex-direction: column; gap: 2px; }
.ops-eyebrow {
  font: 500 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted); letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
}
.ops-title { font-family: var(--font-display); font-size: var(--font-size-md); font-weight: 600; }
.crew-log-filters { display: flex; gap: var(--space-1); flex-wrap: wrap; }
.crew-filter-pill {
  font: 500 var(--font-size-xs) var(--font-mono);
  padding: 4px var(--space-3); border-radius: var(--radius-full);
  border: 1px solid var(--border-glass); background: transparent;
  color: var(--text-muted); cursor: pointer;
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase; transition: all var(--transition-fast);
}
.crew-filter-pill:hover { color: var(--text-primary); border-color: var(--text-muted); }
.crew-filter-pill.active { background: var(--text-primary); color: var(--bg-base); border-color: var(--text-primary); }
.crew-log-table-wrap { max-height: 420px; overflow-y: auto; }
.crew-log-table { width: 100%; border-collapse: collapse; font-family: var(--font-mono); font-size: var(--font-size-xs); }
.crew-log-table th {
  text-align: left; padding: var(--space-1) var(--space-2);
  color: var(--text-muted); font-weight: 500;
  letter-spacing: var(--letter-spacing-wide); text-transform: uppercase;
  border-bottom: 1px solid var(--border-glass); position: sticky; top: 0;
  background: var(--bg-glass-solid);
  z-index: 1;
}
.crew-log-table td {
  padding: var(--space-1) var(--space-2); color: var(--text-primary);
  border-bottom: 1px solid rgba(255,255,255,0.04);
  white-space: nowrap;
}
.crew-log-table td.task-col { max-width: 480px; overflow: hidden; text-overflow: ellipsis; }
.log-agent { font-weight: 600; }
.log-status {
  font-size: 9px; padding: 1px var(--space-2); border-radius: var(--radius-full);
  letter-spacing: var(--letter-spacing-wide); text-transform: uppercase;
}
.log-status.todo { background: rgba(139,92,246,0.15); color: var(--brand-violet-glow); }
.log-status.in_progress { background: rgba(125,211,252,0.15); color: var(--brand-cyan); }
.log-status.done { background: rgba(94,226,181,0.15); color: var(--brand-mint); }
.log-status.blocked { background: rgba(242,109,109,0.15); color: var(--brand-red); }
.log-empty { text-align: center; color: var(--text-muted); opacity: 0.5; padding: var(--space-4) !important; }

.empty { padding: var(--space-4); color: var(--text-muted); font-size: var(--font-size-sm); text-align: center; }
.error-text { color: var(--brand-red); }

@media (max-width: 768px) {
  .crew-status-card { flex-wrap: wrap; }
  .crew-log-table td.task-col { max-width: 200px; }
}
</style>

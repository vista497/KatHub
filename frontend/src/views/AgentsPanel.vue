<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import { useAgentChatStore } from '../stores/agentChatStore'

const agentChat = useAgentChatStore()

// ── Types ────────────────────────────────────────────────────────
interface AgentInfo {
  name: string
  status: string       // "running", "stopped", "idle", etc.
  model: string | null
  active: boolean
}

// ── State ────────────────────────────────────────────────────────
const agents = ref<AgentInfo[]>([])
const loading = ref(false)
const toggling = ref<Set<string>>(new Set())
const error = ref<string | null>(null)
let pollTimer: ReturnType<typeof setInterval> | null = null

// ── API ──────────────────────────────────────────────────────────
async function loadAgents() {
  loading.value = true
  error.value = null
  try {
    const resp = await fetch('/api/agents')
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
    const data = await resp.json()

    // Hermes profiles endpoint returns array or { profiles: [...] }
    const raw: any[] = Array.isArray(data) ? data : (data.profiles || data.data || [])
    agents.value = raw.map((a: any) => ({
      name: a.name || a.id || String(a),
      status: a.status || 'unknown',
      model: a.model || a.current_model || null,
      active: a.active !== false && (a.status === 'running' || a.status === 'active'),
    }))
  } catch (e: any) {
    error.value = e.message || 'Failed to load agents'
    agents.value = []
  } finally {
    loading.value = false
  }
}

async function toggleAgent(name: string) {
  toggling.value.add(name)
  try {
    const resp = await fetch(`/api/agents/${encodeURIComponent(name)}/toggle`, {
      method: 'POST',
    })
    if (!resp.ok) {
      const data = await resp.json().catch(() => ({}))
      throw new Error(data.error || `HTTP ${resp.status}`)
    }
    // Refresh after toggle
    await loadAgents()
  } catch (e: any) {
    console.warn(`Failed to toggle agent "${name}":`, e.message)
    // Still refresh to get current state
    await loadAgents()
  } finally {
    toggling.value.delete(name)
  }
}

function isToggling(name: string): boolean {
  return toggling.value.has(name)
}

// ── Lifecycle ────────────────────────────────────────────────────
onMounted(() => {
  loadAgents()
  pollTimer = setInterval(loadAgents, 5000)
})

onUnmounted(() => {
  if (pollTimer) clearInterval(pollTimer)
})
</script>

<template>
  <div class="agents-panel">
    <h2 class="panel-title">🤖 Агенты</h2>

    <div v-if="loading && agents.length === 0" class="state-msg">Загрузка агентов…</div>
    <div v-else-if="error && agents.length === 0" class="state-msg error-text">{{ error }}</div>

    <div v-if="agents.length === 0 && !loading" class="state-msg muted">
      No agents available
    </div>

    <!-- Agent cards -->
    <div v-for="agent in agents" :key="agent.name" class="agent-card" :class="{ active: agent.active }">
      <div class="agent-main">
        <!-- Status indicator -->
        <span
          class="status-circle"
          :class="agent.active ? 'on' : 'off'"
          :title="agent.status"
        ></span>

        <div class="agent-info">
          <span class="agent-name">{{ agent.name }}</span>
          <span class="agent-model" v-if="agent.model">{{ agent.model }}</span>
          <span class="agent-model muted" v-else>нет модели</span>
        </div>
      </div>

      <!-- Actions: chat + toggle -->
      <div class="agent-actions">
        <button
          class="btn-chat"
          :title="`Открыть чат с ${agent.name}`"
          @click="agentChat.openChat(agent.name)"
        >
          💬
        </button>
        <button
          class="btn-toggle"
          :class="{ on: agent.active }"
          :disabled="isToggling(agent.name)"
          @click="toggleAgent(agent.name)"
        >
          {{ isToggling(agent.name) ? '...' : (agent.active ? 'Stop' : 'Start') }}
        </button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.agents-panel {
  padding: var(--space-4);
}

.panel-title {
  font-size: var(--font-size-lg);
  font-weight: 700;
  color: var(--color-text-primary);
  margin-bottom: var(--space-4);
}

.state-msg {
  padding: var(--space-4);
  color: var(--color-text-secondary);
  text-align: center;
  font-size: var(--font-size-sm);
}

.state-msg.muted {
  color: var(--color-text-muted);
}

.error-text {
  color: #f87171;
}

/* ── Agent card ─────────────────────────────── */
.agent-card {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: var(--space-3);
  background: var(--color-bg-tertiary);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-sm);
  margin-bottom: var(--space-2);
  transition: border-color var(--transition-fast);
}

.agent-card.active {
  border-color: rgba(74, 222, 128, 0.15);
}

.agent-main {
  display: flex;
  align-items: center;
  gap: var(--space-3);
  min-width: 0;
}

/* ── Status circle ──────────────────────────── */
.status-circle {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  flex-shrink: 0;
  transition: all var(--transition-fast);
}

.status-circle.on {
  background: #4ade80;
  box-shadow: 0 0 8px rgba(74, 222, 128, 0.5);
}

.status-circle.off {
  background: #666;
  box-shadow: none;
}

/* ── Agent info ─────────────────────────────── */
.agent-info {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
}

.agent-name {
  font-size: var(--font-size-sm);
  font-weight: 600;
  color: var(--color-text-primary);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.agent-model {
  font-size: var(--font-size-xs);
  color: var(--color-text-secondary);
  font-family: var(--font-mono);
}

.agent-model.muted {
  color: var(--color-text-muted);
  font-style: italic;
}

.agent-actions {
  display: flex;
  align-items: center;
  gap: 6px;
  flex-shrink: 0;
}

/* ── Chat button ────────────────────────────── */
.btn-chat {
  width: 30px;
  height: 30px;
  border-radius: 50%;
  font-size: 14px;
  cursor: pointer;
  transition: all var(--transition-fast);
  border: 1px solid var(--color-border);
  background: transparent;
  color: var(--color-text-secondary);
  display: flex;
  align-items: center;
  justify-content: center;
}

.btn-chat:hover:not(:disabled) {
  border-color: var(--color-accent);
  background: rgba(124, 92, 255, 0.12);
}

/* ── Toggle button ──────────────────────────── */
.btn-toggle {
  padding: 4px 14px;
  border-radius: var(--radius-full);
  font-size: 11px;
  font-weight: 600;
  font-family: var(--font-sans);
  cursor: pointer;
  transition: all var(--transition-fast);
  border: 1px solid var(--color-border);
  background: transparent;
  color: var(--color-text-secondary);
  white-space: nowrap;
}

.btn-toggle:hover:not(:disabled) {
  border-color: var(--color-accent);
  color: var(--color-accent);
}

.btn-toggle.on {
  background: rgba(248, 113, 113, 0.12);
  border-color: rgba(248, 113, 113, 0.3);
  color: #f87171;
}

.btn-toggle.on:hover:not(:disabled) {
  background: rgba(248, 113, 113, 0.25);
  color: #ef4444;
}

.btn-toggle:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}
</style>

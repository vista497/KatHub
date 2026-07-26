<script setup lang="ts">
import { ref, onMounted, onUnmounted, computed } from 'vue'

// ── Types ────────────────────────────────────────────────────────
interface HermesInfo {
  alive: boolean
  url: string
  version: string
  model?: string
}

interface SystemData {
  status: string
  version: string
  uptime: number
  httpPort: number
  wsPort: number
  hermes: HermesInfo
}

// ── State ────────────────────────────────────────────────────────
const system = ref<SystemData | null>(null)
const loading = ref(false)
const error = ref<string | null>(null)
let pollTimer: ReturnType<typeof setInterval> | null = null

// ── Computed ─────────────────────────────────────────────────────
const uptimeFormatted = computed(() => {
  if (!system.value) return ''
  const s = system.value.uptime
  const d = Math.floor(s / 86400)
  const h = Math.floor((s % 86400) / 3600)
  const m = Math.floor((s % 3600) / 60)
  const parts: string[] = []
  if (d > 0) parts.push(`${d}d`)
  if (h > 0) parts.push(`${h}h`)
  if (m > 0 || parts.length === 0) parts.push(`${m}m`)
  return parts.join(' ')
})

// ── API ──────────────────────────────────────────────────────────
async function loadSystem() {
  loading.value = true
  error.value = null
  try {
    const resp = await fetch('/api/system')
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
    const data = await resp.json()
    system.value = data as SystemData
  } catch (e: any) {
    error.value = e.message || 'Failed to load system status'
    system.value = null
  } finally {
    loading.value = false
  }
}

// ── Lifecycle ────────────────────────────────────────────────────
onMounted(() => {
  loadSystem()
  pollTimer = setInterval(loadSystem, 10000) // refresh every 10s
})

onUnmounted(() => {
  if (pollTimer) clearInterval(pollTimer)
})
</script>

<template>
  <div class="system-panel">
    <h2 class="panel-title">📊 System</h2>

    <div v-if="loading && !system" class="state-msg">Loading...</div>
    <div v-else-if="error && !system" class="state-msg error-text">{{ error }}</div>

    <template v-if="system">
      <!-- KatHub version + uptime -->
      <div class="card-row">
        <div class="status-card">
          <span class="card-label">KatHub</span>
          <span class="card-value">v{{ system.version }}</span>
        </div>
        <div class="status-card">
          <span class="card-label">Uptime</span>
          <span class="card-value">{{ uptimeFormatted }}</span>
        </div>
      </div>

      <!-- Hermes API status -->
      <div class="hermes-card" :class="{ dead: !system.hermes.alive }">
        <div class="hermes-header">
          <span class="hermes-title">🔗 Hermes API</span>
          <span class="status-dot" :class="system.hermes.alive ? 'alive' : 'dead'">
            {{ system.hermes.alive ? 'ONLINE' : 'OFFLINE' }}
          </span>
        </div>
        <div class="hermes-details">
          <div class="detail-row">
            <span class="detail-label">URL</span>
            <span class="detail-value mono">{{ system.hermes.url }}</span>
          </div>
          <div class="detail-row">
            <span class="detail-label">Version</span>
            <span class="detail-value">{{ system.hermes.version }}</span>
          </div>
          <div v-if="system.hermes.model" class="detail-row">
            <span class="detail-label">Model</span>
            <span class="detail-value accent">{{ system.hermes.model }}</span>
          </div>
        </div>
      </div>

      <!-- Ports -->
      <div class="card-row ports">
        <div class="port-card">
          <span class="card-label">HTTP Port</span>
          <span class="card-value mono">{{ system.httpPort }}</span>
        </div>
        <div class="port-card">
          <span class="card-label">WebSocket</span>
          <span class="card-value mono">{{ system.wsPort }}</span>
        </div>
      </div>
    </template>
  </div>
</template>

<style scoped>
.system-panel {
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

.error-text {
  color: #f87171;
}

/* ── Card row ───────────────────────────────── */
.card-row {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: var(--space-3);
  margin-bottom: var(--space-3);
}

.card-row.ports {
  margin-top: var(--space-3);
}

.status-card, .port-card {
  background: var(--color-bg-tertiary);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-sm);
  padding: var(--space-3);
  display: flex;
  flex-direction: column;
  gap: var(--space-1);
}

.card-label {
  font-size: var(--font-size-xs);
  color: var(--color-text-muted);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.card-value {
  font-size: var(--font-size-base);
  color: var(--color-text-primary);
  font-weight: 600;
}

.card-value.mono {
  font-family: var(--font-mono);
  color: var(--color-accent-secondary);
}

.card-value.accent {
  color: var(--color-accent);
}

/* ── Hermes card ────────────────────────────── */
.hermes-card {
  background: var(--color-bg-tertiary);
  border: 1px solid rgba(74, 222, 128, 0.2);
  border-radius: var(--radius-md);
  padding: var(--space-4);
  transition: border-color var(--transition-fast);
}

.hermes-card.dead {
  border-color: rgba(248, 113, 113, 0.3);
}

.hermes-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: var(--space-3);
}

.hermes-title {
  font-size: var(--font-size-sm);
  font-weight: 600;
  color: var(--color-text-primary);
}

.status-dot {
  font-size: 10px;
  font-weight: 700;
  letter-spacing: 0.08em;
  padding: 2px 8px;
  border-radius: var(--radius-full);
}

.status-dot.alive {
  background: rgba(74, 222, 128, 0.15);
  color: #4ade80;
}

.status-dot.dead {
  background: rgba(248, 113, 113, 0.15);
  color: #f87171;
}

.hermes-details {
  display: flex;
  flex-direction: column;
  gap: var(--space-2);
}

.detail-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.detail-label {
  font-size: var(--font-size-xs);
  color: var(--color-text-muted);
}

.detail-value {
  font-size: var(--font-size-xs);
  color: var(--color-text-secondary);
}

.detail-value.mono {
  font-family: var(--font-mono);
  font-size: 11px;
}

.detail-value.accent {
  color: var(--color-accent);
  font-weight: 600;
}
</style>

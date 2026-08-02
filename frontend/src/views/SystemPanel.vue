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
    <div class="panel-header">
      <span class="ops-eyebrow">System</span>
      <span class="ops-title">Система</span>
    </div>

    <div v-if="loading && !system" class="state-msg">Загрузка…</div>
    <div v-else-if="error && !system" class="state-msg error-text">{{ error }}</div>

    <template v-if="system">
      <!-- KatHub version + uptime -->
      <div class="card-row">
        <div class="status-card">
          <span class="card-label">KatHub</span>
          <span class="card-value">v{{ system.version }}</span>
        </div>
        <div class="status-card">
          <span class="card-label">Аптайм</span>
          <span class="card-value">{{ uptimeFormatted }}</span>
        </div>
      </div>

      <!-- Hermes API status -->
      <div class="hermes-card" :class="{ dead: !system.hermes.alive }">
        <div class="hermes-header">
          <span class="hermes-title">Hermes API</span>
          <span class="status-dot" :class="system.hermes.alive ? 'alive' : 'dead'">
            {{ system.hermes.alive ? 'ONLINE' : 'OFFLINE' }}
          </span>
        </div>
        <div class="hermes-details">
          <div class="detail-row">
            <span class="detail-label">Адрес</span>
            <span class="detail-value mono">{{ system.hermes.url }}</span>
          </div>
          <div class="detail-row">
            <span class="detail-label">Версия</span>
            <span class="detail-value">{{ system.hermes.version }}</span>
          </div>
          <div v-if="system.hermes.model" class="detail-row">
            <span class="detail-label">Модель</span>
            <span class="detail-value accent">{{ system.hermes.model }}</span>
          </div>
        </div>
      </div>

      <!-- Ports -->
      <div class="card-row ports">
        <div class="port-card">
          <span class="card-label">HTTP-порт</span>
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
  display: flex;
  flex-direction: column;
  gap: var(--space-3);
}

.panel-header {
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

.state-msg {
  padding: var(--space-4);
  color: var(--text-muted);
  text-align: center;
  font-family: var(--font-mono);
  font-size: var(--font-size-xs);
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
  opacity: 0.5;
}

.error-text {
  color: var(--brand-red);
  opacity: 1;
  text-transform: none;
  letter-spacing: normal;
}

/* ── Card row ───────────────────────────────── */
.card-row {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: var(--space-3);
  margin-bottom: var(--space-3);
}

.card-row.ports {
  margin-top: 0;
  padding-bottom: var(--space-2);
}

.status-card, .port-card {
  background: var(--bg-glass);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-lg);
  padding: var(--space-4);
  display: flex;
  flex-direction: column;
  gap: var(--space-1);
}

.card-label {
  font: 500 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: var(--letter-spacing-wide);
}

.card-value {
  font-family: var(--font-display);
  font-size: clamp(20px, 2.5vw, 28px);
  font-weight: 500;
  color: var(--text-primary);
  line-height: 1.1;
}

.card-value.mono {
  font-family: var(--font-mono);
  font-size: var(--font-size-lg);
  color: var(--brand-cyan);
}

.card-value.accent {
  color: var(--brand-violet-glow);
}

/* ── Hermes card ────────────────────────────── */
.hermes-card {
  background: var(--bg-glass);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid rgba(94, 226, 181, 0.2);
  border-radius: var(--radius-lg);
  padding: var(--space-4);
  transition: border-color var(--transition-fast);
}

.hermes-card.dead {
  border-color: rgba(242, 109, 109, 0.3);
}

.hermes-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: var(--space-3);
}

.hermes-title {
  font-family: var(--font-display);
  font-size: 15px;
  font-weight: 600;
  color: var(--text-primary);
}

.status-dot {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font: 500 9px var(--font-mono);
  letter-spacing: var(--letter-spacing-wide);
  padding: 3px var(--space-2);
  border-radius: var(--radius-full);
  text-transform: uppercase;
}

.status-dot::before {
  content: '';
  width: 6px;
  height: 6px;
  border-radius: var(--radius-full);
}

.status-dot.alive {
  background: rgba(94, 226, 181, 0.12);
  color: var(--brand-mint);
  border: 1px solid rgba(94, 226, 181, 0.25);
}

.status-dot.alive::before {
  background: var(--brand-mint);
  box-shadow: 0 0 6px var(--brand-mint);
}

.status-dot.dead {
  background: rgba(242, 109, 109, 0.12);
  color: var(--brand-red);
  border: 1px solid rgba(242, 109, 109, 0.25);
}

.status-dot.dead::before {
  background: var(--brand-red);
  box-shadow: 0 0 6px rgba(242, 109, 109, 0.4);
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
  gap: var(--space-3);
  padding: var(--space-1) 0;
}

.detail-label {
  font: 500 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: var(--letter-spacing-wide);
}

.detail-value {
  font-size: var(--font-size-xs);
  color: var(--text-primary);
  word-break: break-all;
  text-align: right;
}

.detail-value.mono {
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--text-muted);
}

.detail-value.accent {
  color: var(--brand-cyan);
  font-weight: 600;
  font-family: var(--font-mono);
}
</style>

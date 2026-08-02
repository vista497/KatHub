<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'

// ── Types ────────────────────────────────────────────────────────
interface ModelInfo {
  name: string
  provider: string
  available: boolean
}

// ── State ────────────────────────────────────────────────────────
const models = ref<ModelInfo[]>([])
const currentModel = ref<string | null>(null)
const selectedProvider = ref<string | null>(null)
const selectedModel = ref<string | null>(null)
const loading = ref(false)
const switching = ref(false)
const switchResult = ref<string | null>(null)
const switchError = ref<string | null>(null)

// ── Computed ─────────────────────────────────────────────────────
const providers = computed(() => {
  const s = new Set(models.value.map(m => m.provider))
  return [...s].sort()
})

const filteredModels = computed(() => {
  if (!selectedProvider.value) return models.value
  return models.value.filter(m => m.provider === selectedProvider.value)
})

const canSwitch = computed(() =>
  selectedModel.value !== null &&
  selectedModel.value !== currentModel.value &&
  !switching.value
)

// ── API calls ────────────────────────────────────────────────────
async function loadModels() {
  loading.value = true
  try {
    // Fetch full model list
    const resp = await fetch('/api/models')
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
    const data = await resp.json()

    // Hermes API returns shape: { models: [...], providers: {...} } or array
    if (Array.isArray(data)) {
      models.value = data.map((m: any) => ({
        name: m.name || m.id || m.model || String(m),
        provider: m.provider || 'default',
        available: m.available !== false,
      }))
    } else if (data.models && Array.isArray(data.models)) {
      models.value = data.models.map((m: any) => ({
        name: m.name || m.id || m.model || String(m),
        provider: m.provider || 'default',
        available: m.available !== false,
      }))
    }
  } catch (e) {
    console.warn('Failed to load models:', e)
    models.value = []
  } finally {
    loading.value = false
  }
}

async function loadCurrentModel() {
  try {
    const resp = await fetch('/api/models/current')
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
    const data = await resp.json()

    // Response shape: { model: "..." } or { name: "..." } or { id: "..." }
    currentModel.value = data.model || data.name || data.id || null
  } catch (e) {
    console.warn('Failed to load current model:', e)
    currentModel.value = null
  }
}

async function applyModel() {
  if (!selectedModel.value) return
  switchResult.value = null
  switchError.value = null
  switching.value = true

  try {
    const resp = await fetch('/api/models/switch', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ model: selectedModel.value }),
    })
    const data = await resp.json()
    if (!resp.ok) throw new Error(data.error || `HTTP ${resp.status}`)

    currentModel.value = selectedModel.value
    switchResult.value = `Switched to "${selectedModel.value}"`
    setTimeout(() => { switchResult.value = null }, 4000)
  } catch (e: any) {
    switchError.value = e.message || 'Switch failed'
    setTimeout(() => { switchError.value = null }, 5000)
  } finally {
    switching.value = false
  }
}

// ── Init ─────────────────────────────────────────────────────────
onMounted(async () => {
  await Promise.all([loadModels(), loadCurrentModel()])
})
</script>

<template>
  <div class="models-panel">
    <div class="panel-header">
      <span class="ops-eyebrow">Models</span>
      <span class="ops-title">Модели</span>
    </div>

    <!-- Loading -->
    <div v-if="loading" class="state-msg">Загрузка моделей…</div>

    <!-- Empty -->
    <div v-else-if="models.length === 0" class="state-msg muted">
      No models available
    </div>

    <!-- Content -->
    <template v-else>
      <div class="models-card">
        <!-- Provider selector -->
        <label class="field">
          <span class="label-text">Провайдер</span>
          <select
            v-model="selectedProvider"
            class="select"
          >
            <option :value="null">Все провайдеры</option>
            <option
              v-for="p in providers"
              :key="p"
              :value="p"
            >{{ p }}</option>
          </select>
        </label>

        <!-- Model selector -->
        <label class="field">
          <span class="label-text">Модель</span>
          <select
            v-model="selectedModel"
            class="select"
          >
            <option :value="null">Выберите модель…</option>
            <option
              v-for="m in filteredModels"
              :key="`${m.provider}/${m.name}`"
              :value="m.name"
              :class="{ current: m.name === currentModel }"
              :disabled="!m.available"
            >
              {{ m.name }}
              {{ m.name === currentModel ? ' (current)' : '' }}
              {{ !m.available ? ' [offline]' : '' }}
            </option>
          </select>
        </label>

        <!-- Current model indicator -->
        <div v-if="currentModel" class="current-badge">
          <span class="badge-dot active"></span>
          <span class="badge-label">Current</span>
          <strong class="badge-value">{{ currentModel }}</strong>
        </div>
        <div v-else class="current-badge">
          <span class="badge-dot inactive"></span>
          <span class="badge-label">No active model</span>
        </div>

        <!-- Apply button -->
        <button
          class="btn-apply"
          :disabled="!canSwitch"
          @click="applyModel"
        >
          {{ switching ? 'Switching…' : 'Применить' }}
        </button>

        <!-- Feedback -->
        <div v-if="switchResult" class="feedback success">{{ switchResult }}</div>
        <div v-if="switchError" class="feedback error">{{ switchError }}</div>
      </div>
    </template>
  </div>
</template>

<style scoped>
.models-panel {
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

.models-card {
  max-width: 560px;
  background: var(--bg-glass);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-lg);
  padding: var(--space-5);
  display: flex;
  flex-direction: column;
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

/* ── Fields ─────────────────────────────────── */
.field {
  display: flex;
  flex-direction: column;
  gap: var(--space-1);
  margin-bottom: var(--space-4);
}

.label-text {
  font: 600 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: var(--letter-spacing-wide);
}

.select {
  background: var(--bg-glass-solid);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-md);
  color: var(--text-primary);
  padding: var(--space-2) var(--space-3);
  font-size: var(--font-size-sm);
  font-family: var(--font-mono);
  cursor: pointer;
  transition: border-color var(--transition-fast);
}

.select:focus {
  outline: none;
  border-color: var(--brand-violet);
  box-shadow: 0 0 0 2px rgba(139, 92, 246, 0.2);
}

.select option.current {
  color: var(--brand-cyan);
  font-weight: 600;
}

/* ── Current badge ──────────────────────────── */
.current-badge {
  display: flex;
  align-items: center;
  gap: var(--space-2);
  padding: var(--space-2) var(--space-3);
  background: rgba(139, 92, 246, 0.08);
  border: 1px solid rgba(139, 92, 246, 0.2);
  border-radius: var(--radius-md);
  font-size: var(--font-size-sm);
  color: var(--text-muted);
  margin-bottom: var(--space-4);
}

.badge-dot {
  width: 8px;
  height: 8px;
  border-radius: var(--radius-full);
  flex-shrink: 0;
}

.badge-dot.active {
  background: var(--brand-mint);
  box-shadow: 0 0 6px var(--brand-mint);
}

.badge-dot.inactive {
  background: var(--brand-red);
  box-shadow: 0 0 6px rgba(242, 109, 109, 0.4);
}

.badge-label {
  font: 500 9px var(--font-mono);
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
}

.badge-value {
  font-family: var(--font-mono);
  font-size: var(--font-size-xs);
  color: var(--text-primary);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

/* ── Apply button ───────────────────────────── */
.btn-apply {
  width: 100%;
  padding: var(--space-3) var(--space-4);
  background: var(--brand-violet);
  color: #fff;
  border: none;
  border-radius: var(--radius-md);
  font-size: var(--font-size-sm);
  font-weight: 600;
  font-family: var(--font-sans);
  cursor: pointer;
  transition: all var(--transition-fast);
}

.btn-apply:hover:not(:disabled) {
  background: var(--brand-violet-glow);
  box-shadow: 0 0 12px rgba(139, 92, 246, 0.4);
}

.btn-apply:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

/* ── Feedback messages ──────────────────────── */
.feedback {
  margin-top: var(--space-3);
  padding: var(--space-2) var(--space-3);
  border-radius: var(--radius-md);
  font-family: var(--font-mono);
  font-size: var(--font-size-xs);
  font-weight: 500;
}

.feedback.success {
  background: rgba(94, 226, 181, 0.1);
  color: var(--brand-mint);
  border: 1px solid rgba(94, 226, 181, 0.25);
}

.feedback.error {
  background: rgba(242, 109, 109, 0.1);
  color: var(--brand-red);
  border: 1px solid rgba(242, 109, 109, 0.25);
}
</style>

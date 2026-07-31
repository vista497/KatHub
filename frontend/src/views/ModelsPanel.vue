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
    <h2 class="panel-title">🔄 Модели</h2>

    <!-- Loading -->
    <div v-if="loading" class="state-msg">Загрузка моделей…</div>

    <!-- Empty -->
    <div v-else-if="models.length === 0" class="state-msg muted">
      No models available
    </div>

    <!-- Content -->
    <template v-else>
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
        Current: <strong>{{ currentModel }}</strong>
      </div>
      <div v-else class="current-badge">
        <span class="badge-dot inactive"></span>
        No active model
      </div>

      <!-- Apply button -->
      <button
        class="btn-apply"
        :disabled="!canSwitch"
        @click="applyModel"
      >
        {{ switching ? 'Switching...' : 'Apply' }}
      </button>

      <!-- Feedback -->
      <div v-if="switchResult" class="feedback success">{{ switchResult }}</div>
      <div v-if="switchError" class="feedback error">{{ switchError }}</div>
    </template>
  </div>
</template>

<style scoped>
.models-panel {
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

/* ── Fields ─────────────────────────────────── */
.field {
  display: flex;
  flex-direction: column;
  gap: var(--space-1);
  margin-bottom: var(--space-4);
}

.label-text {
  font-size: var(--font-size-xs);
  color: var(--color-text-muted);
  text-transform: uppercase;
  letter-spacing: 0.05em;
  font-weight: 600;
}

.select {
  background: var(--color-bg-tertiary);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-sm);
  color: var(--color-text-primary);
  padding: var(--space-2) var(--space-3);
  font-size: var(--font-size-sm);
  font-family: var(--font-sans);
  cursor: pointer;
  transition: border-color var(--transition-fast);
}

.select:focus {
  outline: none;
  border-color: var(--color-accent);
  box-shadow: 0 0 0 2px rgba(124, 92, 255, 0.2);
}

.select option.current {
  color: var(--color-accent-secondary);
  font-weight: 600;
}

/* ── Current badge ──────────────────────────── */
.current-badge {
  display: flex;
  align-items: center;
  gap: var(--space-2);
  padding: var(--space-2) var(--space-3);
  background: rgba(124, 92, 255, 0.08);
  border: 1px solid rgba(124, 92, 255, 0.15);
  border-radius: var(--radius-sm);
  font-size: var(--font-size-sm);
  color: var(--color-text-secondary);
  margin-bottom: var(--space-4);
}

.badge-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  flex-shrink: 0;
}

.badge-dot.active {
  background: #4ade80;
  box-shadow: 0 0 6px rgba(74, 222, 128, 0.5);
}

.badge-dot.inactive {
  background: #f87171;
  box-shadow: 0 0 6px rgba(248, 113, 113, 0.4);
}

/* ── Apply button ───────────────────────────── */
.btn-apply {
  width: 100%;
  padding: var(--space-3) var(--space-4);
  background: var(--color-accent);
  color: #fff;
  border: none;
  border-radius: var(--radius-sm);
  font-size: var(--font-size-sm);
  font-weight: 600;
  font-family: var(--font-sans);
  cursor: pointer;
  transition: all var(--transition-fast);
}

.btn-apply:hover:not(:disabled) {
  background: #9575ff;
  box-shadow: 0 0 12px rgba(124, 92, 255, 0.4);
}

.btn-apply:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

/* ── Feedback messages ──────────────────────── */
.feedback {
  margin-top: var(--space-3);
  padding: var(--space-2) var(--space-3);
  border-radius: var(--radius-sm);
  font-size: var(--font-size-xs);
  font-weight: 500;
}

.feedback.success {
  background: rgba(74, 222, 128, 0.1);
  color: #4ade80;
  border: 1px solid rgba(74, 222, 128, 0.25);
}

.feedback.error {
  background: rgba(248, 113, 113, 0.1);
  color: #f87171;
  border: 1px solid rgba(248, 113, 113, 0.25);
}
</style>

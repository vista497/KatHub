<script setup lang="ts">
import { ref, onMounted } from 'vue'

interface CronJob {
  id: string
  name: string
  schedule: string
  enabled: boolean
  last_run?: string
  next_run?: string
  prompt?: string
  script?: string
  skills?: string[]
  deliver?: string
}

const jobs = ref<CronJob[]>([])
const loading = ref(false)
const error = ref('')
const showForm = ref(false)
const editingJob = ref<CronJob | null>(null)
const actionLoading = ref<string | null>(null)

// Form state
const form = ref({
  name: '',
  schedule: 'every 1h',
  prompt: '',
  script: '',
  skills: '',
  deliver: '',
  enabled: true,
})

// ── API helpers ──────────────────────────────────────────────
async function api(path: string, opts: RequestInit = {}) {
  const resp = await fetch(path, {
    headers: { 'Content-Type': 'application/json', ...opts.headers },
    ...opts,
  })
  if (!resp.ok) {
    const text = await resp.text()
    throw new Error(`HTTP ${resp.status}: ${text}`)
  }
  return resp.json()
}

// ── Load jobs ────────────────────────────────────────────────
async function loadJobs() {
  loading.value = true
  error.value = ''
  try {
    jobs.value = await api('/api/cron')
  } catch (e: any) {
    error.value = e.message
  } finally {
    loading.value = false
  }
}

// ── Actions ──────────────────────────────────────────────────
async function toggleJob(job: CronJob) {
  actionLoading.value = job.id
  try {
    await api(`/api/cron/${encodeURIComponent(job.id)}/toggle`, { method: 'PATCH' })
    job.enabled = !job.enabled
  } catch (e: any) {
    error.value = e.message
  } finally {
    actionLoading.value = null
  }
}

async function runJob(job: CronJob) {
  actionLoading.value = job.id
  try {
    await api(`/api/cron/${encodeURIComponent(job.id)}/run`, { method: 'POST' })
  } catch (e: any) {
    error.value = e.message
  } finally {
    actionLoading.value = null
  }
}

async function deleteJob(job: CronJob) {
  if (!confirm(`Delete cron job "${job.name || job.id}"?`)) return
  actionLoading.value = job.id
  try {
    await api(`/api/cron/${encodeURIComponent(job.id)}`, { method: 'DELETE' })
    jobs.value = jobs.value.filter(j => j.id !== job.id)
  } catch (e: any) {
    error.value = e.message
  } finally {
    actionLoading.value = null
  }
}

// ── Form ─────────────────────────────────────────────────────
function openCreateForm() {
  editingJob.value = null
  form.value = { name: '', schedule: 'every 1h', prompt: '', script: '', skills: '', deliver: '', enabled: true }
  showForm.value = true
}

function openEditForm(job: CronJob) {
  editingJob.value = job
  form.value = {
    name: job.name || '',
    schedule: job.schedule || '',
    prompt: job.prompt || '',
    script: job.script || '',
    skills: (job.skills || []).join(', '),
    deliver: job.deliver || '',
    enabled: job.enabled ?? true,
  }
  showForm.value = true
}

async function submitForm() {
  const payload: any = {
    name: form.value.name,
    schedule: form.value.schedule,
    prompt: form.value.prompt,
    script: form.value.script || undefined,
    skills: form.value.skills ? form.value.skills.split(',').map(s => s.trim()).filter(Boolean) : undefined,
    deliver: form.value.deliver || undefined,
    enabled: form.value.enabled,
  }

  try {
    if (editingJob.value) {
      // Update — but cron API doesn't have PUT; use POST with same name? 
      // Actually for now just delete + recreate
      error.value = 'Edit not supported via API — delete and recreate'
      return
    } else {
      await api('/api/cron', { method: 'POST', body: JSON.stringify(payload) })
    }
    showForm.value = false
    await loadJobs()
  } catch (e: any) {
    error.value = e.message
  }
}

// ── Helpers ──────────────────────────────────────────────────
function formatTime(ts?: string): string {
  if (!ts) return '—'
  try {
    return new Date(ts).toLocaleString()
  } catch {
    return ts
  }
}

const schedulePresets = [
  'every 5m', 'every 15m', 'every 30m', 'every 1h',
  'every 3h', 'every 6h', 'every 12h', 'every 24h',
  '0 9 * * *', '0 0 * * *',
]

onMounted(loadJobs)
</script>

<template>
  <div class="panel">
    <!-- Header -->
    <div class="panel-header">
      <div class="panel-title-row">
        <router-link to="/" class="back-link">← Back</router-link>
        <h2 class="panel-title">⏱️ Cron Jobs</h2>
      </div>
      <div class="panel-actions">
        <button class="btn btn-primary" @click="openCreateForm">+ New Job</button>
        <button class="btn btn-ghost" @click="loadJobs" :disabled="loading">↻ Refresh</button>
      </div>
    </div>

    <!-- Error -->
    <div v-if="error" class="error-banner">
      {{ error }}
      <button @click="error = ''">✕</button>
    </div>

    <!-- Table -->
    <div class="panel-body">
      <div v-if="loading && jobs.length === 0" class="loading-state">Loading...</div>

      <div v-else-if="jobs.length === 0" class="empty-state">
        <p>No cron jobs configured.</p>
        <button class="btn btn-primary" @click="openCreateForm">+ Create First Job</button>
      </div>

      <div v-else class="table-wrap">
        <table class="cron-table">
          <thead>
            <tr>
              <th>Name</th>
              <th>Schedule</th>
              <th>Status</th>
              <th>Last Run</th>
              <th>Actions</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="job in jobs" :key="job.id" :class="{ disabled: !job.enabled }">
              <td class="name-cell">{{ job.name || job.id }}</td>
              <td><code>{{ job.schedule }}</code></td>
              <td>
                <span class="badge" :class="job.enabled ? 'badge-on' : 'badge-off'">
                  {{ job.enabled ? 'ON' : 'OFF' }}
                </span>
              </td>
              <td class="time-cell">{{ formatTime(job.last_run) }}</td>
              <td class="actions-cell">
                <button
                  class="btn btn-sm"
                  :class="job.enabled ? 'btn-warn' : 'btn-ghost'"
                  @click="toggleJob(job)"
                  :disabled="actionLoading === job.id"
                >
                  {{ job.enabled ? '⏸ Pause' : '▶ Resume' }}
                </button>
                <button
                  class="btn btn-sm btn-ghost"
                  @click="runJob(job)"
                  :disabled="actionLoading === job.id"
                >
                  ▶ Run
                </button>
                <button
                  class="btn btn-sm btn-ghost"
                  @click="openEditForm(job)"
                >
                  ✎
                </button>
                <button
                  class="btn btn-sm btn-danger"
                  @click="deleteJob(job)"
                  :disabled="actionLoading === job.id"
                >
                  ✕
                </button>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- Create/Edit Modal -->
    <Transition name="fade">
      <div v-if="showForm" class="modal-overlay" @click.self="showForm = false">
        <div class="modal">
          <div class="modal-header">
            <h3>{{ editingJob ? 'Edit Job' : 'New Cron Job' }}</h3>
            <button class="btn-close" @click="showForm = false">✕</button>
          </div>
          <div class="modal-body">
            <label>
              Name
              <input v-model="form.name" type="text" placeholder="my-daily-task" />
            </label>
            <label>
              Schedule
              <select v-model="form.schedule">
                <option v-for="p in schedulePresets" :key="p" :value="p">{{ p }}</option>
              </select>
              <input
                v-if="!schedulePresets.includes(form.schedule)"
                v-model="form.schedule"
                type="text"
                placeholder="custom cron / interval"
              />
            </label>
            <label>
              Prompt
              <textarea v-model="form.prompt" rows="4" placeholder="Task prompt for the agent..." />
            </label>
            <label>
              Script (optional)
              <input v-model="form.script" type="text" placeholder="path/to/script.py" />
            </label>
            <label>
              Skills (comma-separated, optional)
              <input v-model="form.skills" type="text" placeholder="my-skill, another-skill" />
            </label>
            <label>
              Deliver (optional)
              <input v-model="form.deliver" type="text" placeholder="telegram, discord, or origin" />
            </label>
            <label class="checkbox-label">
              <input v-model="form.enabled" type="checkbox" />
              Enabled
            </label>
          </div>
          <div class="modal-footer">
            <button class="btn btn-ghost" @click="showForm = false">Cancel</button>
            <button class="btn btn-primary" @click="submitForm" :disabled="!form.name || !form.schedule">
              {{ editingJob ? 'Save' : 'Create' }}
            </button>
          </div>
        </div>
      </div>
    </Transition>
  </div>
</template>

<style scoped>
.panel {
  height: 100vh;
  display: flex;
  flex-direction: column;
  background: var(--color-bg-primary);
  color: var(--color-text-primary);
  font-family: var(--font-sans);
}

/* ── Header ─────────────────────────────────── */
.panel-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 16px 24px;
  background: var(--color-bg-secondary);
  border-bottom: 1px solid var(--color-border);
  flex-shrink: 0;
}

.panel-title-row {
  display: flex;
  align-items: center;
  gap: 12px;
}

.back-link {
  color: var(--color-accent);
  text-decoration: none;
  font-size: 14px;
  padding: 4px 8px;
  border-radius: var(--radius-sm);
  transition: background var(--transition-fast);
}
.back-link:hover { background: var(--color-surface-hover); }

.panel-title {
  font-size: 18px;
  font-weight: 700;
  margin: 0;
}

.panel-actions {
  display: flex;
  gap: 8px;
}

/* ── Body ───────────────────────────────────── */
.panel-body {
  flex: 1;
  overflow-y: auto;
  padding: 24px;
}

/* ── Table ──────────────────────────────────── */
.table-wrap {
  overflow-x: auto;
}

.cron-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 14px;
}

.cron-table th {
  text-align: left;
  padding: 10px 12px;
  color: var(--color-text-muted);
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  border-bottom: 1px solid var(--color-border);
}

.cron-table td {
  padding: 10px 12px;
  border-bottom: 1px solid rgba(255,255,255,0.04);
  vertical-align: middle;
}

.cron-table tr.disabled td {
  opacity: 0.45;
}

.cron-table tr:hover td {
  background: rgba(255,255,255,0.02);
}

.name-cell {
  font-weight: 600;
  max-width: 200px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.time-cell {
  color: var(--color-text-secondary);
  font-size: 13px;
  white-space: nowrap;
}

.actions-cell {
  display: flex;
  gap: 4px;
  flex-wrap: wrap;
}

code {
  background: var(--color-bg-tertiary);
  padding: 2px 6px;
  border-radius: 4px;
  font-family: var(--font-mono);
  font-size: 12px;
}

/* ── Badges ─────────────────────────────────── */
.badge {
  display: inline-block;
  padding: 2px 8px;
  border-radius: var(--radius-full);
  font-size: 11px;
  font-weight: 700;
}

.badge-on {
  background: rgba(92, 224, 130, 0.15);
  color: #5ce082;
}

.badge-off {
  background: rgba(255, 107, 107, 0.15);
  color: #ff6b6b;
}

/* ── States ─────────────────────────────────── */
.loading-state, .empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 16px;
  padding: 48px;
  color: var(--color-text-muted);
}

.error-banner {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 10px 16px;
  margin: 12px 24px;
  background: rgba(255,107,107,0.1);
  border: 1px solid rgba(255,107,107,0.25);
  border-radius: var(--radius-sm);
  color: #ff6b6b;
  font-size: 13px;
}

.error-banner button {
  background: none;
  border: none;
  color: #ff6b6b;
  cursor: pointer;
  font-size: 16px;
  padding: 0 4px;
}

/* ── Buttons ────────────────────────────────── */
.btn {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  padding: 6px 14px;
  border-radius: var(--radius-sm);
  border: 1px solid transparent;
  font-size: 13px;
  font-family: var(--font-sans);
  cursor: pointer;
  transition: all var(--transition-fast);
  white-space: nowrap;
}

.btn:disabled { opacity: 0.4; cursor: not-allowed; }

.btn-primary {
  background: var(--color-accent);
  color: #fff;
  border-color: var(--color-accent);
}
.btn-primary:hover:not(:disabled) { background: #6b4fe0; }

.btn-ghost {
  background: transparent;
  color: var(--color-text-secondary);
  border-color: var(--color-border);
}
.btn-ghost:hover:not(:disabled) { background: var(--color-surface-hover); color: var(--color-text-primary); }

.btn-warn {
  background: rgba(255,179,71,0.1);
  color: #ffb347;
  border-color: rgba(255,179,71,0.25);
}
.btn-warn:hover:not(:disabled) { background: rgba(255,179,71,0.2); }

.btn-danger {
  background: transparent;
  color: #ff6b6b;
  border-color: transparent;
}
.btn-danger:hover:not(:disabled) { background: rgba(255,107,107,0.15); }

.btn-sm { padding: 3px 10px; font-size: 12px; }

/* ── Modal ──────────────────────────────────── */
.modal-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0,0,0,0.6);
  z-index: 300;
  display: flex;
  align-items: center;
  justify-content: center;
}

.modal {
  width: min(560px, 90vw);
  max-height: 85vh;
  background: var(--color-bg-secondary);
  border: 1px solid rgba(124,92,255,0.25);
  border-radius: var(--radius-lg);
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.modal-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 14px 20px;
  border-bottom: 1px solid var(--color-border);
}

.modal-header h3 {
  margin: 0;
  font-size: 16px;
  font-weight: 600;
}

.btn-close {
  background: none;
  border: none;
  color: var(--color-text-muted);
  cursor: pointer;
  font-size: 18px;
  padding: 4px 8px;
  border-radius: 4px;
}
.btn-close:hover { color: #fff; background: rgba(255,255,255,0.08); }

.modal-body {
  flex: 1;
  overflow-y: auto;
  padding: 20px;
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.modal-body label {
  display: flex;
  flex-direction: column;
  gap: 4px;
  font-size: 13px;
  color: var(--color-text-secondary);
}

.modal-body input[type="text"],
.modal-body select,
.modal-body textarea {
  background: var(--color-bg-tertiary);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-sm);
  padding: 8px 12px;
  color: var(--color-text-primary);
  font-family: var(--font-sans);
  font-size: 14px;
  outline: none;
  transition: border-color var(--transition-fast);
}

.modal-body input:focus,
.modal-body select:focus,
.modal-body textarea:focus {
  border-color: var(--color-accent);
}

.modal-body textarea {
  resize: vertical;
  min-height: 80px;
  font-family: var(--font-mono);
  font-size: 13px;
}

.checkbox-label {
  flex-direction: row !important;
  align-items: center;
  gap: 8px !important;
}

.modal-footer {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  padding: 14px 20px;
  border-top: 1px solid var(--color-border);
}

/* ── Transitions ────────────────────────────── */
.fade-enter-active, .fade-leave-active {
  transition: opacity 0.2s;
}
.fade-enter-from, .fade-leave-to { opacity: 0; }
</style>

<script setup lang="ts">
import { ref, onMounted, computed } from 'vue'
import { marked } from 'marked'

interface Skill {
  name: string
  description?: string
  content?: string
  category?: string
  tags?: string[]
}

const skills = ref<Skill[]>([])
const loading = ref(false)
const error = ref('')
const searchQuery = ref('')
const viewingSkill = ref<Skill | null>(null)
const editingSkill = ref<Skill | null>(null)
const showCreateForm = ref(false)
const actionLoading = ref(false)

// Editor state
const editContent = ref('')
const editName = ref('')
const editDescription = ref('')
const editCategory = ref('')

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

// ── Load skills ──────────────────────────────────────────────
async function loadSkills() {
  loading.value = true
  error.value = ''
  try {
    skills.value = await api('/api/skills')
  } catch (e: any) {
    error.value = e.message
  } finally {
    loading.value = false
  }
}

// ── Filtered list ────────────────────────────────────────────
const filteredSkills = computed(() => {
  const q = searchQuery.value.toLowerCase().trim()
  if (!q) return skills.value
  return skills.value.filter(s =>
    s.name.toLowerCase().includes(q) ||
    (s.description || '').toLowerCase().includes(q) ||
    (s.category || '').toLowerCase().includes(q)
  )
})

// ── View skill ───────────────────────────────────────────────
async function viewSkill(skill: Skill) {
  viewingSkill.value = skill
  if (skill.content) return // Already loaded from list

  actionLoading.value = true
  try {
    const data = await api(`/api/skills/${encodeURIComponent(skill.name)}`)
    viewingSkill.value = { ...skill, content: data.content || data.body || JSON.stringify(data) }
  } catch (e: any) {
    error.value = e.message
  } finally {
    actionLoading.value = false
  }
}

// ── Edit skill ───────────────────────────────────────────────
async function startEdit(skill: Skill) {
  if (!skill.content) {
    actionLoading.value = true
    try {
      const data = await api(`/api/skills/${encodeURIComponent(skill.name)}`)
      skill.content = data.content || data.body || ''
    } catch (e: any) {
      error.value = e.message
      return
    } finally {
      actionLoading.value = false
    }
  }
  editingSkill.value = skill
  editContent.value = skill.content || ''
  editName.value = skill.name
  editDescription.value = skill.description || ''
  editCategory.value = skill.category || ''
}

async function saveEdit() {
  if (!editingSkill.value) return
  actionLoading.value = true
  try {
    const payload: any = {
      name: editName.value,
      description: editDescription.value || undefined,
      category: editCategory.value || undefined,
      content: editContent.value,
    }
    await api(`/api/skills/${encodeURIComponent(editingSkill.value.name)}`, {
      method: 'PUT',
      body: JSON.stringify(payload),
    })
    // Refresh local state
    const idx = skills.value.findIndex(s => s.name === editingSkill.value!.name)
    if (idx >= 0) {
      skills.value[idx] = {
        ...skills.value[idx],
        name: editName.value,
        description: editDescription.value,
        category: editCategory.value,
        content: editContent.value,
      }
    }
    if (viewingSkill.value?.name === editingSkill.value.name) {
      viewingSkill.value = {
        ...viewingSkill.value,
        name: editName.value,
        description: editDescription.value,
        category: editCategory.value,
        content: editContent.value,
      }
    }
    editingSkill.value = null
  } catch (e: any) {
    error.value = e.message
  } finally {
    actionLoading.value = false
  }
}

function cancelEdit() {
  editingSkill.value = null
}

// ── Create skill ─────────────────────────────────────────────
function openCreateForm() {
  editName.value = ''
  editDescription.value = ''
  editCategory.value = ''
  editContent.value = '# New Skill\n\nWrite your skill content here...\n'
  showCreateForm.value = true
}

async function createSkill() {
  actionLoading.value = true
  try {
    const payload: any = {
      name: editName.value,
      description: editDescription.value || undefined,
      category: editCategory.value || undefined,
      content: editContent.value,
    }
    await api('/api/skills', {
      method: 'POST',
      body: JSON.stringify(payload),
    })
    showCreateForm.value = false
    await loadSkills()
  } catch (e: any) {
    error.value = e.message
  } finally {
    actionLoading.value = false
  }
}

// ── Delete skill ─────────────────────────────────────────────
async function deleteSkill(skill: Skill) {
  if (!confirm(`Delete skill "${skill.name}"?`)) return
  actionLoading.value = true
  try {
    await api(`/api/skills/${encodeURIComponent(skill.name)}`, { method: 'DELETE' })
    skills.value = skills.value.filter(s => s.name !== skill.name)
    if (viewingSkill.value?.name === skill.name) viewingSkill.value = null
  } catch (e: any) {
    error.value = e.message
  } finally {
    actionLoading.value = false
  }
}

// ── Render markdown ──────────────────────────────────────────
function renderMarkdown(content: string): string {
  if (!content) return '<em>Нет содержимого</em>'
  try {
    return marked.parse(content) as string
  } catch {
    return `<pre>${escapeHtml(content)}</pre>`
  }
}

function escapeHtml(s: string): string {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
}

function closeViewer() {
  viewingSkill.value = null
  editingSkill.value = null
}

// ── Lifecycle ────────────────────────────────────────────────
onMounted(loadSkills)
</script>

<template>
  <div class="panel">
    <!-- Header -->
    <div class="panel-header">
      <div class="panel-title-row">
        <router-link to="/" class="back-link">←</router-link>
        <div class="panel-header-left">
          <span class="ops-eyebrow">Skills</span>
          <span class="ops-title">Навыки</span>
        </div>
      </div>
      <div class="panel-actions">
        <button class="btn btn-primary" @click="openCreateForm">+ New Skill</button>
        <button class="btn btn-ghost" @click="loadSkills" :disabled="loading">↻ Refresh</button>
      </div>
    </div>

    <!-- Error -->
    <div v-if="error" class="error-banner">
      {{ error }}
      <button @click="error = ''">✕</button>
    </div>

    <div class="panel-body">
      <!-- Detail view -->
      <div v-if="viewingSkill" class="detail-view">
        <div class="detail-header">
          <button class="btn btn-ghost btn-sm" @click="closeViewer">← Back to list</button>
          <div class="detail-title-row">
            <h3>{{ viewingSkill.name }}</h3>
            <span v-if="viewingSkill.category" class="category-tag">{{ viewingSkill.category }}</span>
          </div>
          <p v-if="viewingSkill.description" class="detail-desc">{{ viewingSkill.description }}</p>
          <div class="detail-actions">
            <button
              class="btn btn-ghost btn-sm"
              @click="startEdit(viewingSkill)"
              v-if="!editingSkill"
            >✎ Edit</button>
            <button class="btn btn-danger btn-sm" @click="deleteSkill(viewingSkill)">✕ Delete</button>
          </div>
        </div>

        <!-- Edit mode -->
        <div v-if="editingSkill" class="editor-area">
          <div class="editor-fields">
            <label>
              Name
              <input v-model="editName" type="text" />
            </label>
            <label>
              Description
              <input v-model="editDescription" type="text" placeholder="Short description..." />
            </label>
            <label>
              Category
              <input v-model="editCategory" type="text" placeholder="e.g. devops, mlops" />
            </label>
          </div>
          <label class="editor-label">
            Content (Markdown)
            <textarea
              v-model="editContent"
              class="editor-textarea"
              rows="20"
              spellcheck="false"
            />
          </label>
          <div class="editor-footer">
            <button class="btn btn-ghost" @click="cancelEdit">Отмена</button>
            <button class="btn btn-primary" @click="saveEdit" :disabled="actionLoading">
              {{ actionLoading ? 'Saving...' : 'Save' }}
            </button>
          </div>
        </div>

        <!-- View mode -->
        <div v-else class="markdown-body" v-html="renderMarkdown(viewingSkill.content || '')" />
      </div>

      <!-- List view -->
      <template v-else>
        <!-- Search -->
        <div class="search-bar">
          <input
            v-model="searchQuery"
            type="text"
            placeholder="Search skills..."
            class="search-input"
          />
        </div>

        <!-- Loading -->
        <div v-if="loading && skills.length === 0" class="loading-state">Загрузка…</div>

        <!-- Empty -->
        <div v-else-if="skills.length === 0" class="empty-state">
          <p>Навыки не найдены.</p>
          <button class="btn btn-primary" @click="openCreateForm">+ Create First Skill</button>
        </div>

        <!-- List -->
        <div v-else class="skills-list">
          <div v-if="filteredSkills.length === 0 && searchQuery" class="empty-state">
            No skills matching "{{ searchQuery }}"
          </div>
          <div
            v-for="skill in filteredSkills"
            :key="skill.name"
            class="skill-card"
            @click="viewSkill(skill)"
          >
            <div class="skill-card-header">
              <span class="skill-name">{{ skill.name }}</span>
              <span v-if="skill.category" class="category-tag">{{ skill.category }}</span>
            </div>
            <p v-if="skill.description" class="skill-desc">{{ skill.description }}</p>
          </div>
        </div>
      </template>
    </div>

    <!-- Create Modal -->
    <Transition name="fade">
      <div v-if="showCreateForm" class="modal-overlay" @click.self="showCreateForm = false">
        <div class="modal">
          <div class="modal-header">
            <h3>Новый навык</h3>
            <button class="btn-close" @click="showCreateForm = false">✕</button>
          </div>
          <div class="modal-body">
            <label>
              Name
              <input v-model="editName" type="text" placeholder="my-skill" />
            </label>
            <label>
              Description
              <input v-model="editDescription" type="text" placeholder="Short description..." />
            </label>
            <label>
              Category
              <input v-model="editCategory" type="text" placeholder="e.g. devops, mlops" />
            </label>
            <label>
              Content (Markdown)
              <textarea v-model="editContent" rows="15" class="editor-textarea" spellcheck="false" />
            </label>
          </div>
          <div class="modal-footer">
            <button class="btn btn-ghost" @click="showCreateForm = false">Отмена</button>
            <button class="btn btn-primary" @click="createSkill" :disabled="!editName.trim() || actionLoading">
              {{ actionLoading ? 'Creating...' : 'Create' }}
            </button>
          </div>
        </div>
      </div>
    </Transition>
  </div>
</template>

<style scoped>
.panel {
  height: 100%;
  display: flex;
  flex-direction: column;
  color: var(--text-primary);
  font-family: var(--font-sans);
}

/* ── Header ─────────────────────────────────── */
.panel-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: var(--space-4);
  border-bottom: 1px solid var(--border-glass);
  flex-shrink: 0;
  gap: var(--space-3);
}

.panel-title-row {
  display: flex;
  align-items: center;
  gap: var(--space-2);
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

.back-link {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 30px;
  height: 30px;
  color: var(--text-muted);
  text-decoration: none;
  font-family: var(--font-mono);
  font-size: 13px;
  border: 1px solid var(--border-glass);
  background: var(--bg-glass);
  border-radius: var(--radius-full);
  transition: all var(--transition-fast);
}
.back-link:hover {
  border-color: var(--brand-violet);
  color: var(--brand-violet-glow);
  background: rgba(139, 92, 246, 0.08);
}

.panel-actions {
  display: flex;
  gap: var(--space-2);
}

/* ── Body ───────────────────────────────────── */
.panel-body {
  flex: 1;
  overflow-y: auto;
  padding: var(--space-4);
}

/* ── Search ─────────────────────────────────── */
.search-bar {
  margin-bottom: var(--space-4);
}

.search-input {
  width: 100%;
  background: var(--bg-glass-solid);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-full);
  padding: var(--space-2) var(--space-4);
  color: var(--text-primary);
  font-family: var(--font-mono);
  font-size: var(--font-size-sm);
  outline: none;
  transition: border-color var(--transition-fast);
}

.search-input:focus {
  border-color: var(--brand-violet);
  box-shadow: 0 0 0 2px rgba(139, 92, 246, 0.2);
}

.search-input::placeholder {
  color: var(--text-muted);
  opacity: 0.6;
}

/* ── Skills list ────────────────────────────── */
.skills-list {
  display: flex;
  flex-direction: column;
  gap: var(--space-2);
}

.skill-card {
  padding: var(--space-3) var(--space-4);
  background: var(--bg-glass);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-lg);
  cursor: pointer;
  transition: all var(--transition-fast);
}

.skill-card:hover {
  border-color: rgba(139, 92, 246, 0.4);
  background: rgba(139, 92, 246, 0.05);
}

.skill-card-header {
  display: flex;
  align-items: center;
  gap: var(--space-2);
  margin-bottom: 4px;
}

.skill-name {
  font-weight: 600;
  font-size: 13px;
  font-family: var(--font-mono);
  color: var(--brand-violet-glow);
}

.skill-desc {
  font-size: 13px;
  color: var(--text-muted);
  margin: 0;
  line-height: 1.4;
}

.category-tag {
  display: inline-block;
  padding: 1px var(--space-2);
  background: rgba(125, 211, 252, 0.1);
  color: var(--brand-cyan);
  border: 1px solid rgba(125, 211, 252, 0.2);
  border-radius: var(--radius-full);
  font: 500 10px var(--font-mono);
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
}

/* ── Detail view ────────────────────────────── */
.detail-view {
  max-width: 900px;
  margin: 0 auto;
}

.detail-header {
  margin-bottom: var(--space-4);
  padding-bottom: var(--space-4);
  border-bottom: 1px solid var(--border-glass);
}

.detail-title-row {
  display: flex;
  align-items: center;
  gap: var(--space-3);
  margin: var(--space-3) 0 var(--space-2);
}

.detail-title-row h3 {
  margin: 0;
  font-size: clamp(18px, 2.5vw, 24px);
  font-weight: 600;
  font-family: var(--font-mono);
  color: var(--brand-violet-glow);
}

.detail-desc {
  color: var(--text-muted);
  font-size: 14px;
  margin: 4px 0 var(--space-3);
}

.detail-actions {
  display: flex;
  gap: var(--space-2);
  margin-top: var(--space-2);
}

/* ── Editor ─────────────────────────────────── */
.editor-area {
  display: flex;
  flex-direction: column;
  gap: var(--space-4);
}

.editor-fields {
  display: flex;
  gap: var(--space-3);
  flex-wrap: wrap;
}

.editor-fields label {
  flex: 1;
  min-width: 180px;
  display: flex;
  flex-direction: column;
  gap: var(--space-1);
  font: 500 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: var(--letter-spacing-wide);
}

.editor-fields input {
  background: var(--bg-glass-solid);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-md);
  padding: var(--space-2) var(--space-3);
  color: var(--text-primary);
  font-family: var(--font-mono);
  font-size: var(--font-size-sm);
  outline: none;
  transition: border-color var(--transition-fast);
}
.editor-fields input:focus { border-color: var(--brand-violet); }

.editor-label {
  display: flex;
  flex-direction: column;
  gap: var(--space-1);
  font: 500 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: var(--letter-spacing-wide);
}

.editor-textarea {
  background: var(--bg-glass-solid);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-md);
  padding: var(--space-3);
  color: var(--text-primary);
  font-family: var(--font-mono);
  font-size: 13px;
  line-height: 1.6;
  outline: none;
  resize: vertical;
}
.editor-textarea:focus { border-color: var(--brand-violet); }

.editor-footer {
  display: flex;
  justify-content: flex-end;
  gap: var(--space-2);
  padding-top: var(--space-2);
}

/* ── Markdown rendered ──────────────────────── */
.markdown-body {
  padding: var(--space-2) 0;
  line-height: 1.7;
  color: var(--text-primary);
  font-size: 14px;
}

.markdown-body :deep(h1) { font-size: 1.5em; margin: 1em 0 0.5em; color: var(--brand-violet-glow); }
.markdown-body :deep(h2) { font-size: 1.3em; margin: 1em 0 0.4em; color: var(--text-primary); }
.markdown-body :deep(h3) { font-size: 1.1em; margin: 0.8em 0 0.3em; }
.markdown-body :deep(p) { margin: 0.5em 0; }
.markdown-body :deep(code) {
  background: var(--bg-glass-solid);
  padding: 2px 6px;
  border-radius: 4px;
  font-family: var(--font-mono);
  font-size: 13px;
  color: var(--brand-cyan);
}
.markdown-body :deep(pre) {
  background: var(--bg-glass-solid);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-md);
  padding: var(--space-3) var(--space-4);
  overflow-x: auto;
  margin: 0.8em 0;
}
.markdown-body :deep(pre code) {
  background: none;
  padding: 0;
}
.markdown-body :deep(ul), .markdown-body :deep(ol) {
  padding-left: 1.5em;
  margin: 0.5em 0;
}
.markdown-body :deep(li) { margin: 0.25em 0; }
.markdown-body :deep(blockquote) {
  border-left: 3px solid var(--brand-violet);
  padding: 4px 12px;
  margin: 0.5em 0;
  color: var(--text-muted);
  background: rgba(139, 92, 246, 0.05);
  border-radius: 0 var(--radius-md) var(--radius-md) 0;
}
.markdown-body :deep(a) { color: var(--brand-cyan); }
.markdown-body :deep(table) {
  border-collapse: collapse;
  width: 100%;
  margin: 0.8em 0;
}
.markdown-body :deep(th), .markdown-body :deep(td) {
  border: 1px solid var(--border-glass);
  padding: 6px 12px;
  text-align: left;
  font-size: 13px;
}
.markdown-body :deep(th) { background: var(--bg-glass-solid); }
.markdown-body :deep(hr) { border: none; border-top: 1px solid var(--border-glass); margin: 1em 0; }

/* ── States ─────────────────────────────────── */
.loading-state, .empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: var(--space-4);
  padding: var(--space-8);
  color: var(--text-muted);
  font-family: var(--font-mono);
  font-size: var(--font-size-xs);
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
  opacity: 0.5;
}

.error-banner {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: var(--space-2) var(--space-4);
  margin: 0 var(--space-4) var(--space-3);
  background: rgba(242, 109, 109, 0.1);
  border: 1px solid rgba(242, 109, 109, 0.25);
  border-radius: var(--radius-md);
  color: var(--brand-red);
  font-size: 13px;
}

.error-banner button {
  background: none;
  border: none;
  color: var(--brand-red);
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
  border-radius: var(--radius-full);
  border: 1px solid transparent;
  font-size: var(--font-size-xs);
  font-family: var(--font-mono);
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
  cursor: pointer;
  transition: all var(--transition-fast);
  white-space: nowrap;
}

.btn:disabled { opacity: 0.4; cursor: not-allowed; }

.btn-primary {
  background: var(--brand-violet);
  color: #fff;
  border-color: var(--brand-violet);
}
.btn-primary:hover:not(:disabled) {
  background: var(--brand-violet-glow);
  box-shadow: 0 0 12px rgba(139, 92, 246, 0.4);
}

.btn-ghost {
  background: transparent;
  color: var(--text-muted);
  border-color: var(--border-glass);
}
.btn-ghost:hover:not(:disabled) {
  border-color: var(--brand-violet);
  color: var(--brand-violet-glow);
  background: rgba(139, 92, 246, 0.06);
}

.btn-danger {
  background: transparent;
  color: var(--brand-red);
  border-color: transparent;
}
.btn-danger:hover:not(:disabled) { background: rgba(242, 109, 109, 0.15); }

.btn-sm { padding: 3px 10px; font-size: 11px; }

/* ── Modal ──────────────────────────────────── */
.modal-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.55);
  backdrop-filter: blur(4px);
  -webkit-backdrop-filter: blur(4px);
  z-index: 300;
  display: flex;
  align-items: center;
  justify-content: center;
}

.modal {
  width: min(640px, 90vw);
  max-height: 85vh;
  background: var(--bg-glass-solid);
  border: 1px solid rgba(139, 92, 246, 0.3);
  border-radius: var(--radius-lg);
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.modal-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: var(--space-3) var(--space-5);
  border-bottom: 1px solid var(--border-glass);
}

.modal-header h3 {
  margin: 0;
  font-family: var(--font-display);
  font-size: 16px;
  font-weight: 600;
  color: var(--text-primary);
}

.btn-close {
  background: none;
  border: 1px solid transparent;
  color: var(--text-muted);
  cursor: pointer;
  font-size: 18px;
  padding: 2px 8px;
  border-radius: var(--radius-full);
  transition: all var(--transition-fast);
}
.btn-close:hover {
  color: #fff;
  border-color: var(--border-glass);
  background: rgba(255, 255, 255, 0.06);
}

.modal-body {
  flex: 1;
  overflow-y: auto;
  padding: var(--space-5);
  display: flex;
  flex-direction: column;
  gap: var(--space-3);
}

.modal-body label {
  display: flex;
  flex-direction: column;
  gap: var(--space-1);
  font: 500 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: var(--letter-spacing-wide);
}

.modal-body input[type="text"],
.modal-body textarea {
  background: var(--bg-glass);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-md);
  padding: var(--space-2) var(--space-3);
  color: var(--text-primary);
  font-family: var(--font-mono);
  font-size: var(--font-size-sm);
  outline: none;
  transition: border-color var(--transition-fast);
}

.modal-body input:focus,
.modal-body textarea:focus {
  border-color: var(--brand-violet);
}

.modal-body textarea {
  resize: vertical;
  font-size: 13px;
}

.modal-footer {
  display: flex;
  justify-content: flex-end;
  gap: var(--space-2);
  padding: var(--space-3) var(--space-5);
  border-top: 1px solid var(--border-glass);
}

/* ── Transitions ────────────────────────────── */
.fade-enter-active, .fade-leave-active {
  transition: opacity 0.2s;
}
.fade-enter-from, .fade-leave-to { opacity: 0; }
</style>

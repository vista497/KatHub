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
  if (!content) return '<em>No content</em>'
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
        <router-link to="/" class="back-link">← Back</router-link>
        <h2 class="panel-title">🧩 Skills</h2>
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
            <button class="btn btn-ghost" @click="cancelEdit">Cancel</button>
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
        <div v-if="loading && skills.length === 0" class="loading-state">Loading...</div>

        <!-- Empty -->
        <div v-else-if="skills.length === 0" class="empty-state">
          <p>No skills found.</p>
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
            <h3>New Skill</h3>
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
            <button class="btn btn-ghost" @click="showCreateForm = false">Cancel</button>
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

/* ── Search ─────────────────────────────────── */
.search-bar {
  margin-bottom: 20px;
}

.search-input {
  width: 100%;
  background: var(--color-bg-tertiary);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-sm);
  padding: 10px 16px;
  color: var(--color-text-primary);
  font-family: var(--font-sans);
  font-size: 14px;
  outline: none;
  transition: border-color var(--transition-fast);
}

.search-input:focus {
  border-color: var(--color-accent);
}

.search-input::placeholder {
  color: var(--color-text-muted);
}

/* ── Skills list ────────────────────────────── */
.skills-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.skill-card {
  padding: 14px 18px;
  background: var(--color-bg-secondary);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-md);
  cursor: pointer;
  transition: all var(--transition-fast);
}

.skill-card:hover {
  border-color: rgba(124,92,255,0.3);
  background: rgba(124,92,255,0.04);
}

.skill-card-header {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 4px;
}

.skill-name {
  font-weight: 600;
  font-size: 15px;
  font-family: var(--font-mono);
  color: var(--color-accent);
}

.skill-desc {
  font-size: 13px;
  color: var(--color-text-secondary);
  margin: 0;
  line-height: 1.4;
}

.category-tag {
  display: inline-block;
  padding: 1px 8px;
  background: rgba(92,224,255,0.1);
  color: var(--color-accent-secondary);
  border-radius: var(--radius-full);
  font-size: 11px;
  font-weight: 600;
}

/* ── Detail view ────────────────────────────── */
.detail-view {
  max-width: 900px;
  margin: 0 auto;
}

.detail-header {
  margin-bottom: 24px;
  padding-bottom: 16px;
  border-bottom: 1px solid var(--color-border);
}

.detail-title-row {
  display: flex;
  align-items: center;
  gap: 12px;
  margin: 12px 0 8px;
}

.detail-title-row h3 {
  margin: 0;
  font-size: 20px;
  font-weight: 700;
  font-family: var(--font-mono);
  color: var(--color-accent);
}

.detail-desc {
  color: var(--color-text-secondary);
  font-size: 14px;
  margin: 4px 0 12px;
}

.detail-actions {
  display: flex;
  gap: 8px;
  margin-top: 8px;
}

/* ── Editor ─────────────────────────────────── */
.editor-area {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.editor-fields {
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
}

.editor-fields label {
  flex: 1;
  min-width: 180px;
  display: flex;
  flex-direction: column;
  gap: 4px;
  font-size: 13px;
  color: var(--color-text-secondary);
}

.editor-fields input {
  background: var(--color-bg-tertiary);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-sm);
  padding: 8px 12px;
  color: var(--color-text-primary);
  font-family: var(--font-sans);
  font-size: 14px;
  outline: none;
}
.editor-fields input:focus { border-color: var(--color-accent); }

.editor-label {
  display: flex;
  flex-direction: column;
  gap: 4px;
  font-size: 13px;
  color: var(--color-text-secondary);
}

.editor-textarea {
  background: var(--color-bg-tertiary);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-sm);
  padding: 12px;
  color: var(--color-text-primary);
  font-family: var(--font-mono);
  font-size: 13px;
  line-height: 1.6;
  outline: none;
  resize: vertical;
}
.editor-textarea:focus { border-color: var(--color-accent); }

.editor-footer {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  padding-top: 8px;
}

/* ── Markdown rendered ──────────────────────── */
.markdown-body {
  padding: 8px 0;
  line-height: 1.7;
  color: #ccccee;
  font-size: 14px;
}

.markdown-body :deep(h1) { font-size: 1.5em; margin: 1em 0 0.5em; color: var(--color-accent); }
.markdown-body :deep(h2) { font-size: 1.3em; margin: 1em 0 0.4em; color: #ccccee; }
.markdown-body :deep(h3) { font-size: 1.1em; margin: 0.8em 0 0.3em; }
.markdown-body :deep(p) { margin: 0.5em 0; }
.markdown-body :deep(code) {
  background: var(--color-bg-tertiary);
  padding: 2px 6px;
  border-radius: 4px;
  font-family: var(--font-mono);
  font-size: 13px;
}
.markdown-body :deep(pre) {
  background: var(--color-bg-tertiary);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-sm);
  padding: 12px 16px;
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
  border-left: 3px solid var(--color-accent);
  padding: 4px 12px;
  margin: 0.5em 0;
  color: var(--color-text-secondary);
  background: rgba(124,92,255,0.05);
  border-radius: 0 var(--radius-sm) var(--radius-sm) 0;
}
.markdown-body :deep(a) { color: var(--color-accent-secondary); }
.markdown-body :deep(table) {
  border-collapse: collapse;
  width: 100%;
  margin: 0.8em 0;
}
.markdown-body :deep(th), .markdown-body :deep(td) {
  border: 1px solid var(--color-border);
  padding: 6px 12px;
  text-align: left;
  font-size: 13px;
}
.markdown-body :deep(th) { background: var(--color-bg-tertiary); }
.markdown-body :deep(hr) { border: none; border-top: 1px solid var(--color-border); margin: 1em 0; }

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
  width: min(640px, 90vw);
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
.modal-body textarea:focus {
  border-color: var(--color-accent);
}

.modal-body textarea {
  resize: vertical;
  font-family: var(--font-mono);
  font-size: 13px;
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

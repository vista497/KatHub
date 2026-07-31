<script setup lang="ts">
import { ref, onMounted, computed } from 'vue'

interface VaultNode {
  id: string
  path?: string
  name?: string
  label?: string
  type?: string
}

const vaultNodes = ref<VaultNode[]>([])
const selectedFile = ref<{ path: string; title: string; content: string } | null>(null)
const selectedId = ref<string | null>(null)
const fileLoading = ref(false)
const fileError = ref<string | null>(null)

const ROOT_RU: Record<string, string> = {
  'Диалоги': 'Диалоги',
  'Проекты': 'Проекты',
  'Агенты': 'Агенты',
  '_meta': '_meta',
}

function rootFolder(path: string): string {
  const parts = path.split('/')
  return parts.length > 1 ? parts[0] : '—'
}
function vaultFolder(path: string): string {
  const parts = path.split('/')
  return parts.length > 1 ? parts.slice(0, -1).join('/') : '—'
}

// Группируем по корневым папкам
const grouped = computed(() => {
  const map = new Map<string, VaultNode[]>()
  vaultNodes.value.forEach(n => {
    const root = rootFolder(n.path || n.id)
    if (!map.has(root)) map.set(root, [])
    map.get(root)!.push(n)
  })
  return [...map.entries()].sort((a, b) => a[0].localeCompare(b[0]))
})

async function loadVault() {
  try {
    const resp = await fetch('/api/vault/graph')
    if (!resp.ok) return
    const data = await resp.json()
    const nodes: any[] = Array.isArray(data) ? data : (data.nodes || [])
    vaultNodes.value = nodes.filter((n: any) => n.type === 'note' || n.type === 'file')
  } catch { /* тихо */ }
}

async function openFile(node: VaultNode) {
  const path = node.path || node.id
  if (!path) return
  selectedId.value = node.id
  selectedFile.value = null
  fileLoading.value = true
  fileError.value = null
  try {
    const resp = await fetch('/api/vault/file?path=' + encodeURIComponent(path))
    if (!resp.ok) throw new Error('HTTP ' + resp.status)
    const data = await resp.json()
    selectedFile.value = {
      path,
      title: node.label || node.name || path.split('/').pop() || path,
      content: data.content || data.text || JSON.stringify(data, null, 2),
    }
  } catch (e: any) {
    fileError.value = e.message || 'Не удалось открыть файл'
  } finally {
    fileLoading.value = false
  }
}

onMounted(loadVault)
</script>

<template>
  <div class="content-layout">
    <!-- Sidebar -->
    <div class="content-sidebar">
      <div class="content-sidebar-header">
        <span class="ops-eyebrow">Content</span>
        <span class="ops-title">Obsidian Vault</span>
      </div>
      <div v-if="vaultNodes.length === 0" class="content-sidebar-empty">Vault пуст или недоступен</div>
      <div v-for="[root, nodes] in grouped" :key="root" class="content-sidebar-group">
        <div class="content-sidebar-agent-header">{{ ROOT_RU[root] || root }}</div>
        <div
          v-for="n in nodes"
          :key="n.id"
          class="content-sidebar-row"
          :class="{ selected: selectedId === n.id }"
          @click="openFile(n)"
        >
          <div class="doc-title">{{ n.label || n.name || (n.path || n.id).split('/').pop() }}</div>
          <div class="doc-filename">{{ vaultFolder(n.path || n.id) }}</div>
        </div>
      </div>
    </div>

    <!-- Preview -->
    <div class="content-preview">
      <div v-if="fileLoading" class="content-empty">Загрузка…</div>
      <div v-else-if="fileError" class="content-empty error-text">{{ fileError }}</div>
      <div v-else-if="selectedFile" class="preview-box">
        <div class="preview-header">
          <span class="preview-title">{{ selectedFile.title }}</span>
          <button class="preview-close" @click="selectedFile = null">✕</button>
        </div>
        <pre class="preview-content">{{ selectedFile.content }}</pre>
      </div>
      <div v-else class="content-empty">Выберите заметку слева</div>
    </div>
  </div>
</template>

<style scoped>
.content-layout {
  display: flex;
  min-height: 100%;
  height: 100%;
}

/* Sidebar */
.content-sidebar {
  width: 300px;
  flex-shrink: 0;
  background: var(--bg-glass);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-lg) 0 0 var(--radius-lg);
  overflow-y: auto;
  padding: var(--space-3) 0;
}
.content-sidebar-header {
  display: flex;
  flex-direction: column;
  gap: 2px;
  padding: var(--space-2) var(--space-4) var(--space-3);
  border-bottom: 1px solid var(--border-glass);
  margin-bottom: var(--space-2);
}
.ops-eyebrow {
  font: 500 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted); letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
}
.ops-title { font-family: var(--font-display); font-size: var(--font-size-md); font-weight: 600; color: var(--text-primary); }

.content-sidebar-empty {
  padding: var(--space-6) var(--space-4);
  color: var(--text-muted); font-family: var(--font-mono);
  font-size: var(--font-size-xs); letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase; text-align: center; opacity: 0.35;
}
.content-sidebar-group { margin-bottom: var(--space-3); }
.content-sidebar-agent-header {
  padding: var(--space-2) var(--space-4);
  font-family: var(--font-mono); font-size: var(--font-size-xs);
  font-weight: 600; letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase; color: var(--text-muted);
  position: sticky; top: 0;
  background: var(--bg-glass-solid);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  z-index: 1;
}
.content-sidebar-row {
  padding: var(--space-2) var(--space-4);
  cursor: pointer; transition: background var(--transition-fast);
  border-left: 2px solid transparent;
}
.content-sidebar-row:hover { background: rgba(255,255,255,0.04); }
.content-sidebar-row.selected { background: rgba(255,255,255,0.06); border-left-color: var(--brand-violet); }
.doc-title {
  font-family: var(--font-display); font-size: 13px; font-weight: 500;
  color: var(--text-primary); line-height: 1.3; margin-bottom: 2px;
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
}
.doc-filename {
  font-family: var(--font-mono); font-size: 10px;
  color: var(--text-muted); opacity: 0.6;
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
}

/* Preview */
.content-preview {
  flex: 1; min-width: 0;
  background: var(--bg-glass);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass);
  border-left: none;
  border-radius: 0 var(--radius-lg) var(--radius-lg) 0;
  overflow-y: auto;
  padding: var(--space-5);
}
.content-empty {
  display: flex; align-items: center; justify-content: center;
  height: 100%; min-height: 200px;
  color: var(--text-muted); font-family: var(--font-mono);
  font-size: var(--font-size-xs); letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase; opacity: 0.35;
}
.error-text { color: var(--brand-red); opacity: 1; }

.preview-box {
  display: flex; flex-direction: column;
  height: 100%;
  background: var(--bg-glass-solid);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-md);
  overflow: hidden;
}
.preview-header {
  display: flex; align-items: center; justify-content: space-between;
  padding: var(--space-2) var(--space-3);
  border-bottom: 1px solid var(--border-glass);
  font-size: var(--font-size-sm);
}
.preview-title {
  font-family: var(--font-display); font-weight: 600;
  color: var(--text-primary);
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
}
.preview-close {
  background: none; border: none; color: var(--text-muted);
  cursor: pointer; font-size: 15px;
}
.preview-close:hover { color: var(--text-primary); }
.preview-content {
  flex: 1; overflow-y: auto; padding: var(--space-3);
  margin: 0; font-family: var(--font-mono);
  font-size: 12px; line-height: 1.6; color: var(--text-primary);
  white-space: pre-wrap; word-break: break-word;
}

@media (max-width: 800px) {
  .content-layout { flex-direction: column; }
  .content-sidebar { width: 100%; border-radius: var(--radius-lg); border-right: 1px solid var(--border-glass); }
  .content-preview { border-radius: var(--radius-lg); border-left: 1px solid var(--border-glass); }
}
</style>

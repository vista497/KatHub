<script setup lang="ts">
import { computed, defineAsyncComponent } from 'vue'
import { useRoute } from 'vue-router'
import SidebarPanel from '../components/layout/SidebarPanel.vue'
import GalaxyGraph from '../components/graph/GalaxyGraph.vue'
import ChatOverlay from '../components/chat/ChatOverlay.vue'
import { useGraphStore } from '../stores/graphStore'

// Lazy-load panel components
const CronPanel = defineAsyncComponent(() => import('./CronPanel.vue'))
const SkillsPanel = defineAsyncComponent(() => import('./SkillsPanel.vue'))
const ModelsPanel = defineAsyncComponent(() => import('./ModelsPanel.vue'))
const SystemPanel = defineAsyncComponent(() => import('./SystemPanel.vue'))
const AgentsPanel = defineAsyncComponent(() => import('./AgentsPanel.vue'))
const KanbanPanel = defineAsyncComponent(() => import('./KanbanPanel.vue'))

const route = useRoute()
const graph = useGraphStore()

// Map route paths to panel components
const panelMap: Record<string, any> = {
  '/settings/cron': CronPanel,
  '/settings/skills': SkillsPanel,
  '/settings/models': ModelsPanel,
  '/settings/system': SystemPanel,
  '/settings/agents': AgentsPanel,
  '/settings/kanban': KanbanPanel,
}

const activePanel = computed(() => panelMap[route.path] || null)
const showGalaxy = computed(() => !activePanel.value)

// Tab definitions for navigation
const tabs = [
  { path: '/settings/cron', label: 'Cron' },
  { path: '/settings/skills', label: 'Skills' },
  { path: '/settings/models', label: 'Models' },
  { path: '/settings/system', label: 'System' },
  { path: '/settings/agents', label: 'Agents' },
  { path: '/settings/kanban', label: 'Kanban' },
]
</script>

<template>
  <div class="desktop-layout">
    <SidebarPanel class="sidebar" asLinks />
    <div class="main-area">
      <!-- Tab navigation bar -->
      <nav class="tab-bar" v-if="activePanel">
        <router-link
          v-for="tab in tabs"
          :key="tab.path"
          :to="tab.path"
          class="tab"
          :class="{ active: route.path === tab.path }"
        >
          {{ tab.label }}
        </router-link>
      </nav>

      <!-- Content area -->
      <div class="content" :class="{ 'has-tabs': activePanel }">
        <GalaxyGraph v-if="showGalaxy" class="graph" />
        <component v-else :is="activePanel" class="panel" />
      </div>
    </div>

    <!-- Right edge: chat toggle + slide-out panel -->
    <ChatOverlay class="chat-overlay" />

    <!-- File editor modal -->
    <Transition name="fade">
      <div v-if="graph.editingFile" class="file-editor-overlay">
        <div class="file-editor">
          <div class="editor-header">
            <span class="editor-title">📄 {{ graph.editingTitle }}</span>
            <button class="editor-close" @click="graph.closeFile()">✕</button>
          </div>
          <div v-if="graph.fileLoading" class="editor-loading">Loading...</div>
          <pre v-else class="editor-content">{{ graph.fileContent }}</pre>
        </div>
      </div>
    </Transition>
  </div>
</template>

<style scoped>
.desktop-layout {
  display: grid;
  grid-template-columns: var(--sidebar-width) 1fr 0px;
  grid-template-rows: 100vh;
  overflow: hidden;
  background: var(--color-bg-primary);
  color: var(--color-text-primary);
  font-family: var(--font-sans);
}

.main-area {
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

/* Tab bar */
.tab-bar {
  display: flex;
  gap: 0;
  background: var(--color-bg-secondary);
  border-bottom: 1px solid var(--color-border);
  padding: 0 var(--space-3);
  flex-shrink: 0;
  min-height: 40px;
}

.tab {
  padding: var(--space-2) var(--space-4);
  font-size: var(--font-size-sm);
  color: var(--color-text-secondary);
  text-decoration: none;
  border-bottom: 2px solid transparent;
  transition: all var(--transition-fast);
  white-space: nowrap;
}

.tab:hover {
  color: var(--color-text-primary);
  background: var(--color-surface-hover);
}

.tab.active {
  color: var(--color-accent);
  border-bottom-color: var(--color-accent);
}

/* Content */
.content {
  flex: 1;
  overflow: hidden;
  position: relative;
}

.content.has-tabs {
  /* panels get full height minus tab bar */
}

.graph {
  width: 100%;
  height: 100%;
}

.panel {
  width: 100%;
  height: 100%;
  overflow-y: auto;
}

.chat-overlay {
  height: 100vh;
  overflow: visible;
}

/* File editor modal */
.file-editor-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.6);
  z-index: 200;
  display: flex;
  align-items: center;
  justify-content: center;
}

.file-editor {
  width: min(800px, 90vw);
  max-height: 80vh;
  background: #0e0e1a;
  border: 1px solid rgba(124, 92, 255, 0.3);
  border-radius: 12px;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.editor-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 16px;
  border-bottom: 1px solid rgba(124, 92, 255, 0.15);
}

.editor-title {
  font-size: 14px;
  color: #ccccee;
  font-weight: 600;
}

.editor-close {
  background: none;
  border: none;
  color: #888a;
  cursor: pointer;
  font-size: 18px;
  padding: 4px 8px;
  border-radius: 4px;
  transition: all 0.15s;
}
.editor-close:hover { color: #fff; background: rgba(255,255,255,0.08); }

.editor-loading {
  padding: 24px;
  color: #666888;
  text-align: center;
}

.editor-content {
  flex: 1;
  overflow-y: auto;
  padding: 16px;
  margin: 0;
  font-family: var(--font-mono, monospace);
  font-size: 13px;
  line-height: 1.6;
  color: #ccccee;
  white-space: pre-wrap;
  word-break: break-word;
}

.fade-enter-active, .fade-leave-active {
  transition: opacity 0.2s;
}
.fade-enter-from, .fade-leave-to {
  opacity: 0;
}

@media (max-width: 1023px) {
  .desktop-layout {
    grid-template-columns: 1fr;
  }
  .desktop-layout .sidebar {
    display: none;
  }
}
</style>

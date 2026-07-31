<script setup lang="ts">
import { computed, defineAsyncComponent } from 'vue'
import { useRoute } from 'vue-router'
import SidebarPanel from '../components/layout/SidebarPanel.vue'
import DashboardView from './DashboardView.vue'
import ChatOverlay from '../components/chat/ChatOverlay.vue'
import AgentChatModal from '../components/chat/AgentChatModal.vue'
import { useGraphStore } from '../stores/graphStore'

// Lazy-load panel components (панели референса + управление)
const CrewPanel = defineAsyncComponent(() => import('./CrewPanel.vue'))
const KanbanPanel = defineAsyncComponent(() => import('./KanbanPanel.vue'))
const CronPanel = defineAsyncComponent(() => import('./CronPanel.vue'))
const ContentPanel = defineAsyncComponent(() => import('./ContentPanel.vue'))
const GalaxyGraph = defineAsyncComponent(() => import('../components/graph/GalaxyGraph.vue'))
const SkillsPanel = defineAsyncComponent(() => import('./SkillsPanel.vue'))
const ModelsPanel = defineAsyncComponent(() => import('./ModelsPanel.vue'))
const SystemPanel = defineAsyncComponent(() => import('./SystemPanel.vue'))
const AgentsPanel = defineAsyncComponent(() => import('./AgentsPanel.vue'))

const route = useRoute()
const graph = useGraphStore()

// Map route paths to panel components
const panelMap: Record<string, any> = {
  '/crew': CrewPanel,
  '/kanban': KanbanPanel,
  '/cron': CronPanel,
  '/content': ContentPanel,
  '/galaxy': GalaxyGraph,
  '/skills': SkillsPanel,
  '/models': ModelsPanel,
  '/system': SystemPanel,
  '/agents': AgentsPanel,
}

const activePanel = computed(() => panelMap[route.path] || null)
const showDashboard = computed(() => !activePanel.value)
</script>

<template>
  <div class="desktop-layout">
    <SidebarPanel class="sidebar" asLinks />
    <div class="main-area">
      <!-- Content area -->
      <div class="content">
        <DashboardView v-if="showDashboard" class="dashboard" />
        <component v-else :is="activePanel" class="panel" />
      </div>
    </div>

    <!-- Right edge: chat toggle + slide-out panel -->
    <ChatOverlay class="chat-overlay" />

    <!-- Per-agent chat modal -->
    <AgentChatModal />

    <!-- File editor modal -->
    <Transition name="fade">
      <div v-if="graph.editingFile" class="file-editor-overlay">
        <div class="file-editor">
          <div class="editor-header">
            <span class="editor-title">📄 {{ graph.editingTitle }}</span>
            <button class="editor-close" @click="graph.closeFile()">✕</button>
          </div>
          <div v-if="graph.fileLoading" class="editor-loading">Загрузка…</div>
          <pre v-else class="editor-content">{{ graph.fileContent }}</pre>
        </div>
      </div>
    </Transition>
  </div>
</template>

<style scoped>
.desktop-layout {
  display: grid;
  grid-template-columns: var(--sidebar-width) 1fr;
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

/* Content */
.content {
  flex: 1;
  overflow: hidden;
  position: relative;
}

.dashboard {
  width: 100%;
  height: 100%;
}

.panel {
  width: 100%;
  height: 100%;
  overflow-y: auto;
}

.chat-overlay {
  position: fixed;
  right: 0;
  top: 0;
  height: 100vh;
  overflow: visible;
  z-index: 90;
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

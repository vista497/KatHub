<script setup lang="ts">
import SidebarPanel from '../components/layout/SidebarPanel.vue'
import GalaxyGraph from '../components/graph/GalaxyGraph.vue'
import ChatOverlay from '../components/chat/ChatOverlay.vue'
import { useGraphStore } from '../stores/graphStore'

const graph = useGraphStore()
</script>

<template>
  <div class="desktop-layout">
    <SidebarPanel class="sidebar" asLinks />
    <GalaxyGraph class="graph" />

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

.chat-overlay {
  height: 100vh;
  overflow: visible;
  /* The panel itself is positioned absolute inside, so it overflows */
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

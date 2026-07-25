<script setup lang="ts">
import SidebarPanel from '../components/layout/SidebarPanel.vue'
import GalaxyGraph from '../components/graph/GalaxyGraph.vue'
import ChatOverlay from '../components/chat/ChatOverlay.vue'
</script>

<template>
  <div class="desktop-layout">
    <SidebarPanel class="sidebar" />
    <GalaxyGraph class="graph" />
    <ChatOverlay class="chat-overlay" />
  </div>
</template>

<style scoped>
.desktop-layout {
  display: grid;
  grid-template-columns: var(--sidebar-width) 1fr auto;
  grid-template-rows: 100vh;
  overflow: hidden;
  background: var(--color-bg-primary);
  color: var(--color-text-primary);
  font-family: var(--font-sans);
}

@media (max-width: 1023px) {
  .desktop-layout {
    display: none;
  }
}
</style>

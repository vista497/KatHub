<script setup lang="ts">
import { ref } from 'vue'
import ChatView from '../components/chat/ChatView.vue'
import GalaxyGraph from '../components/graph/GalaxyGraph.vue'
import SidebarPanel from '../components/layout/SidebarPanel.vue'

const activeTab = ref<'chat' | 'graph' | 'sidebar'>('chat')
</script>

<template>
  <div class="mobile-layout">
    <div class="mobile-content">
      <ChatView v-if="activeTab === 'chat'" />
      <GalaxyGraph v-else-if="activeTab === 'graph'" />
      <SidebarPanel v-else-if="activeTab === 'sidebar'" />
    </div>
    <nav class="bottom-nav">
      <button :class="{ active: activeTab === 'chat' }" @click="activeTab = 'chat'">
        💬
      </button>
      <button :class="{ active: activeTab === 'graph' }" @click="activeTab = 'graph'">
        ◎
      </button>
      <button :class="{ active: activeTab === 'sidebar' }" @click="activeTab = 'sidebar'">
        ≡
      </button>
    </nav>
  </div>
</template>

<style scoped>
@media (min-width: 1024px) {
  .mobile-layout {
    display: none;
  }
}

.mobile-layout {
  height: 100dvh;
  display: flex;
  flex-direction: column;
  background: var(--color-bg-primary);
  color: var(--color-text-primary);
}

.mobile-content {
  flex: 1;
  overflow: hidden;
}

.bottom-nav {
  display: flex;
  justify-content: space-around;
  padding: var(--space-2);
  background: var(--color-bg-secondary);
  border-top: 1px solid var(--color-border);
}

.bottom-nav button {
  background: none;
  border: none;
  font-size: 1.5rem;
  padding: var(--space-2) var(--space-4);
  border-radius: var(--radius-md);
  cursor: pointer;
  transition: background var(--transition-fast);
  color: var(--color-text-secondary);
}

.bottom-nav button.active {
  background: var(--color-accent-glow);
  color: var(--color-text-primary);
}
</style>

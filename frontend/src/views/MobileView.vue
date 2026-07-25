<script setup lang="ts">
import { ref } from 'vue'
import ChatView from '../components/chat/ChatView.vue'
import GalaxyGraph from '../components/graph/GalaxyGraph.vue'
import SidebarPanel from '../components/layout/SidebarPanel.vue'

const activeTab = ref<'chat' | 'graph' | 'sidebar'>('chat')
const settingsPage = ref<string | null>(null)

function handleNavigate(path: string) {
  settingsPage.value = path
  activeTab.value = 'sidebar'
}
</script>

<template>
  <div class="mobile-layout">
    <div class="mobile-content">
      <!-- Settings sub-pages -->
      <div v-if="activeTab === 'sidebar' && settingsPage" class="settings-page">
        <div class="settings-page-header">
          <button class="back-btn" @click="settingsPage = null">← Back</button>
          <span class="settings-page-title">{{ settingsPage }}</span>
        </div>
        <div class="settings-page-body">
          <div v-if="settingsPage === '/settings/plugins'" class="placeholder-page">
            ⚙️ Plugins — coming soon
          </div>
          <div v-else-if="settingsPage === '/settings/backends'" class="placeholder-page">
            🔌 Backends — coming soon
          </div>
          <div v-else-if="settingsPage === '/settings/theme'" class="placeholder-page">
            🎨 Theme — coming soon
          </div>
        </div>
      </div>

      <!-- Main tabs -->
      <template v-else>
        <ChatView v-if="activeTab === 'chat'" />
        <GalaxyGraph v-else-if="activeTab === 'graph'" />
        <SidebarPanel v-else-if="activeTab === 'sidebar'" @navigate="handleNavigate" />
      </template>
    </div>

    <nav class="bottom-nav">
      <button :class="{ active: activeTab === 'chat' }" @click="activeTab = 'chat'; settingsPage = null">
        💬
      </button>
      <button :class="{ active: activeTab === 'graph' }" @click="activeTab = 'graph'; settingsPage = null">
        ◎
      </button>
      <button :class="{ active: activeTab === 'sidebar' }" @click="activeTab = 'sidebar'; settingsPage = null">
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
  position: relative;
}

/* ── Bottom nav ──────────────────────────────── */
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
  padding: var(--space-2) var(--space-6);
  border-radius: var(--radius-md);
  cursor: pointer;
  transition: background var(--transition-fast);
  color: var(--color-text-secondary);
}

.bottom-nav button.active {
  background: var(--color-accent-glow);
  color: var(--color-text-primary);
}

/* ── Settings sub-pages ──────────────────────── */
.settings-page {
  height: 100%;
  display: flex;
  flex-direction: column;
}

.settings-page-header {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 16px;
  background: var(--color-bg-secondary);
  border-bottom: 1px solid var(--color-border);
}

.back-btn {
  background: none;
  border: none;
  color: var(--color-accent);
  font-size: 14px;
  cursor: pointer;
  padding: 4px 8px;
  font-family: var(--font-sans);
}

.settings-page-title {
  font-size: 14px;
  color: var(--color-text-secondary);
  font-weight: 600;
}

.settings-page-body {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
}

.placeholder-page {
  color: var(--color-text-muted);
  font-size: 16px;
  text-align: center;
  padding: 24px;
}
</style>

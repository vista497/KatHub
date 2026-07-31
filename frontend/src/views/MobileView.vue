<script setup lang="ts">
import { ref, computed, defineAsyncComponent } from 'vue'
import ChatView from '../components/chat/ChatView.vue'
import DashboardView from './DashboardView.vue'
import SidebarPanel from '../components/layout/SidebarPanel.vue'

const activeTab = ref<'chat' | 'graph' | 'sidebar'>('chat')
const settingsPage = ref<string | null>(null)

// Панели — те же, что в DesktopView (ленивая загрузка)
const panelMap: Record<string, any> = {
  '/crew': defineAsyncComponent(() => import('./CrewPanel.vue')),
  '/kanban': defineAsyncComponent(() => import('./KanbanPanel.vue')),
  '/cron': defineAsyncComponent(() => import('./CronPanel.vue')),
  '/content': defineAsyncComponent(() => import('./ContentPanel.vue')),
  '/galaxy': defineAsyncComponent(() => import('../components/graph/GalaxyGraph.vue')),
  '/skills': defineAsyncComponent(() => import('./SkillsPanel.vue')),
  '/models': defineAsyncComponent(() => import('./ModelsPanel.vue')),
  '/system': defineAsyncComponent(() => import('./SystemPanel.vue')),
  '/agents': defineAsyncComponent(() => import('./AgentsPanel.vue')),
}

const currentPanel = computed(() => (settingsPage.value ? panelMap[settingsPage.value] || null : null))

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
          <button class="back-btn" @click="settingsPage = null">← Назад</button>
          <span class="settings-page-title">{{ settingsPage }}</span>
        </div>
        <div class="settings-page-body">
          <component :is="currentPanel" v-if="currentPanel" />
          <div v-else class="placeholder-page">Панель недоступна</div>
        </div>
      </div>

      <!-- Main tabs -->
      <template v-else>
        <ChatView v-if="activeTab === 'chat'" />
        <DashboardView v-else-if="activeTab === 'graph'" />
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
  padding: 6px 8px;
  padding-bottom: calc(6px + env(safe-area-inset-bottom, 0px));
  background: var(--color-bg-secondary);
  border-top: 1px solid var(--color-border);
}

.bottom-nav button {
  background: none;
  border: none;
  font-size: 1.3rem;
  padding: 8px 20px;
  border-radius: var(--radius-md);
  cursor: pointer;
  transition: background var(--transition-fast);
  color: var(--color-text-secondary);
  -webkit-tap-highlight-color: transparent;
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
  overflow: hidden;
}

.placeholder-page {
  color: var(--color-text-muted);
  font-size: 16px;
  text-align: center;
  padding: 24px;
}
</style>

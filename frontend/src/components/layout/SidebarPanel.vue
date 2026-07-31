<script setup lang="ts">
import { useSidebarStore } from '../../stores/sidebarStore'
import SidebarSection from './SidebarSection.vue'

const sidebar = useSidebarStore()

const props = withDefaults(defineProps<{
  asLinks?: boolean  // true = use router-link (desktop), false = emit (mobile)
}>(), {
  asLinks: false
})

const emit = defineEmits<{
  (e: 'navigate', path: string): void
}>()

function handleNav(path: string) {
  emit('navigate', path)
}
</script>

<template>
  <aside class="sidebar" :class="{ collapsed: sidebar.collapsed }">
    <div class="sidebar-header">
      <span class="logo" v-if="!sidebar.collapsed">✦ KatHub</span>
      <button class="collapse-btn" @click="sidebar.toggle">
        {{ sidebar.collapsed ? '▶' : '◀' }}
      </button>
    </div>

    <div class="sidebar-content" v-if="!sidebar.collapsed">
      <!-- Панели (референс) -->
      <SidebarSection title="Панели" name="panels">
        <div class="settings-list">
          <router-link v-if="asLinks" to="/">🏠 Дашборд</router-link>
          <button v-else @click="handleNav('/')">🏠 Дашборд</button>

          <router-link v-if="asLinks" to="/crew">👥 Команда</router-link>
          <button v-else @click="handleNav('/crew')">👥 Команда</button>

          <router-link v-if="asLinks" to="/kanban">✅ Задачи</router-link>
          <button v-else @click="handleNav('/kanban')">✅ Задачи</button>

          <router-link v-if="asLinks" to="/cron">⏱️ Расписание</router-link>
          <button v-else @click="handleNav('/cron')">⏱️ Расписание</button>

          <router-link v-if="asLinks" to="/content">📄 Контент</router-link>
          <button v-else @click="handleNav('/content')">📄 Контент</button>

          <router-link v-if="asLinks" to="/galaxy">🌌 Галактика</router-link>
          <button v-else @click="handleNav('/galaxy')">🌌 Галактика</button>
        </div>
      </SidebarSection>

      <!-- Управление -->
      <SidebarSection title="Управление" name="management">
        <div class="settings-list">
          <router-link v-if="asLinks" to="/skills">🧩 Навыки</router-link>
          <button v-else @click="handleNav('/skills')">🧩 Навыки</button>

          <router-link v-if="asLinks" to="/models">🤖 Модели</router-link>
          <button v-else @click="handleNav('/models')">🤖 Модели</button>

          <router-link v-if="asLinks" to="/system">🖥️ Система</router-link>
          <button v-else @click="handleNav('/system')">🖥️ Система</button>

          <router-link v-if="asLinks" to="/agents">👤 Агенты</router-link>
          <button v-else @click="handleNav('/agents')">👤 Агенты</button>
        </div>
      </SidebarSection>
    </div>
  </aside>
</template>

<style scoped>
.sidebar {
  width: var(--sidebar-width);
  height: 100vh;
  background: var(--bg-glass);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border-right: 1px solid var(--border-glass);
  display: flex;
  flex-direction: column;
  transition: width var(--transition-normal);
  overflow: hidden;
}

.sidebar.collapsed {
  width: 40px;
}

.sidebar-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: var(--space-4);
  border-bottom: 1px solid var(--color-border);
  min-height: 52px;
}

.logo {
  font-family: var(--font-display);
  font-size: var(--font-size-lg);
  font-weight: 700;
  color: var(--text-primary);
  white-space: nowrap;
  letter-spacing: -0.02em;
}
.logo::first-letter { color: var(--brand-violet); }

.collapse-btn {
  background: none;
  border: none;
  color: var(--color-text-secondary);
  cursor: pointer;
  font-size: var(--font-size-sm);
  padding: var(--space-1);
  border-radius: var(--radius-sm);
  transition: all var(--transition-fast);
}

.collapse-btn:hover {
  color: var(--color-text-primary);
  background: var(--color-surface-hover);
}

.sidebar-content {
  flex: 1;
  overflow-y: auto;
  padding: var(--space-2);
}

.settings-list {
  display: flex;
  flex-direction: column;
  gap: var(--space-1);
}

.settings-list a,
.settings-list button {
  color: var(--color-text-secondary);
  text-decoration: none;
  padding: var(--space-2) var(--space-3);
  border-radius: var(--radius-sm);
  font-size: var(--font-size-sm);
  transition: all var(--transition-fast);
  background: none;
  border: none;
  font-family: var(--font-sans);
  text-align: left;
  cursor: pointer;
  width: 100%;
}

.settings-list a:hover,
.settings-list button:hover,
.settings-list button:active {
  background: var(--color-surface-hover);
  color: var(--color-text-primary);
}

.settings-list a.router-link-active {
  background: rgba(139, 92, 246, 0.15);
  color: var(--brand-violet-glow);
  font-weight: 600;
}
</style>

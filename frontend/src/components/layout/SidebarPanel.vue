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
      <SidebarSection title="Settings" name="settings">
        <div class="settings-list">
          <!-- Desktop: router-link | Mobile: button+emit -->
          <router-link v-if="asLinks" to="/settings/plugins">⚙️ Plugins</router-link>
          <button v-else @click="handleNav('/settings/plugins')">⚙️ Plugins</button>

          <router-link v-if="asLinks" to="/settings/backends">🔌 Backends</router-link>
          <button v-else @click="handleNav('/settings/backends')">🔌 Backends</button>

          <router-link v-if="asLinks" to="/settings/theme">🎨 Theme</router-link>
          <button v-else @click="handleNav('/settings/theme')">🎨 Theme</button>
        </div>
      </SidebarSection>
    </div>
  </aside>
</template>

<style scoped>
.sidebar {
  width: var(--sidebar-width);
  height: 100vh;
  background: var(--color-bg-secondary);
  border-right: 1px solid var(--color-border);
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
  font-size: var(--font-size-lg);
  font-weight: 700;
  color: var(--color-accent);
  white-space: nowrap;
}

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

.settings-list a {
  color: var(--color-text-secondary);
  text-decoration: none;
  padding: var(--space-2) var(--space-3);
  border-radius: var(--radius-sm);
  font-size: var(--font-size-sm);
  transition: all var(--transition-fast);
}

.settings-list a:hover {
  background: var(--color-surface-hover);
  color: var(--color-text-primary);
}

.settings-list button {
  background: none;
  border: none;
  color: var(--color-text-secondary);
  text-decoration: none;
  padding: var(--space-3) var(--space-3);
  border-radius: var(--radius-sm);
  font-size: var(--font-size-sm);
  font-family: var(--font-sans);
  text-align: left;
  cursor: pointer;
  transition: all var(--transition-fast);
  width: 100%;
}

.settings-list button:hover,
.settings-list button:active {
  background: var(--color-surface-hover);
  color: var(--color-text-primary);
}
</style>

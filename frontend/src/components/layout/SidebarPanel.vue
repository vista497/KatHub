<script setup lang="ts">
import { useSidebarStore } from '../../stores/sidebarStore'
import SidebarSection from './SidebarSection.vue'
import VaultTree from './VaultTree.vue'
import AiSessions from './AiSessions.vue'

const sidebar = useSidebarStore()
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
      <SidebarSection title="Vault" name="vault">
        <VaultTree />
      </SidebarSection>

      <SidebarSection title="AI" name="ai">
        <AiSessions />
      </SidebarSection>

      <SidebarSection title="Settings" name="settings">
        <div class="settings-list">
          <router-link to="/settings/plugins">⚙️ Plugins</router-link>
          <router-link to="/settings/backends">🔌 Backends</router-link>
          <router-link to="/settings/theme">🎨 Theme</router-link>
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
</style>

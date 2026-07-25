<script setup lang="ts">
import { useSidebarStore } from '../../stores/sidebarStore'

const props = defineProps<{ title: string; name: string }>()
const sidebar = useSidebarStore()
</script>

<template>
  <div class="section">
    <button class="section-header" @click="sidebar.toggleSection(name)">
      <span class="arrow">{{ sidebar.isExpanded(name) ? '▾' : '▸' }}</span>
      <span class="title">{{ title }}</span>
    </button>
    <div class="section-body" v-show="sidebar.isExpanded(name)">
      <slot />
    </div>
  </div>
</template>

<style scoped>
.section {
  margin-bottom: var(--space-1);
}

.section-header {
  display: flex;
  align-items: center;
  gap: var(--space-2);
  width: 100%;
  padding: var(--space-2) var(--space-3);
  background: none;
  border: none;
  color: var(--color-text-secondary);
  cursor: pointer;
  font-size: var(--font-size-xs);
  text-transform: uppercase;
  letter-spacing: 0.05em;
  font-family: var(--font-sans);
  border-radius: var(--radius-sm);
  transition: all var(--transition-fast);
}

.section-header:hover {
  color: var(--color-text-primary);
  background: var(--color-surface-hover);
}

.arrow {
  font-size: 10px;
  width: 12px;
  color: var(--color-text-muted);
}

.section-body {
  padding: var(--space-1) 0 var(--space-2) var(--space-6);
}
</style>

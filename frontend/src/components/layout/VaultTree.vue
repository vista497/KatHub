<script setup lang="ts">
interface Folder {
  name: string
  count: number
}

const folders: Folder[] = [
  { name: 'Диалоги', count: 12 },
  { name: 'Проекты', count: 3 },
  { name: '_meta', count: 5 },
  { name: 'Агенты', count: 4 },
]
</script>

<template>
  <div class="vault-tree">
    <div
      v-for="folder in folders"
      :key="folder.name"
      class="folder-item"
    >
      <span class="folder-icon">📁</span>
      <span class="folder-name">{{ folder.name }}</span>
      <span class="count">{{ folder.count }}</span>
    </div>
  </div>
</template>

<style scoped>
.folder-item {
  display: flex;
  align-items: center;
  gap: var(--space-2);
  padding: var(--space-1) var(--space-2);
  font-size: var(--font-size-sm);
  color: var(--color-text-secondary);
  border-radius: var(--radius-sm);
  cursor: pointer;
  transition: all var(--transition-fast);
}

.folder-item:hover {
  background: var(--color-surface-hover);
  color: var(--color-text-primary);
}

.folder-icon {
  font-size: var(--font-size-sm);
}

.folder-name {
  flex: 1;
}

.count {
  font-size: var(--font-size-xs);
  color: var(--color-text-muted);
  background: var(--color-surface);
  padding: 1px 6px;
  border-radius: var(--radius-full);
}
</style>

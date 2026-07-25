<script setup lang="ts">
const agents = [
  { name: 'Катя', active: true },
  { name: 'twin', active: false },
  { name: 'reflection', active: false },
  { name: 'doctor', active: false },
]
</script>

<template>
  <div class="ai-sessions">
    <div
      v-for="agent in agents"
      :key="agent.name"
      class="agent-item"
      :class="{ active: agent.active }"
    >
      <span class="dot" :class="{ active: agent.active }"></span>
      <span>{{ agent.name }}</span>
    </div>
  </div>
</template>

<style scoped>
.agent-item {
  display: flex;
  align-items: center;
  gap: var(--space-2);
  padding: var(--space-2) var(--space-3);
  font-size: var(--font-size-sm);
  color: var(--color-text-secondary);
  border-radius: var(--radius-sm);
  cursor: pointer;
  transition: all var(--transition-fast);
}

.agent-item:hover {
  background: var(--color-surface-hover);
  color: var(--color-text-primary);
}

.agent-item.active {
  color: var(--color-accent-secondary);
}

.dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: var(--color-text-muted);
  transition: background var(--transition-fast);
}

.dot.active {
  background: var(--color-accent-secondary);
  box-shadow: 0 0 6px var(--color-accent-secondary);
}
</style>

<template>
  <span class="status-badge" :class="badgeClass">
    <span class="status-badge__dot"></span>
    <span class="status-badge__label"><slot>{{ badgeText }}</slot></span>
  </span>
</template>

<script setup lang="ts">
import { computed } from 'vue'

const props = withDefaults(defineProps<{
  status?: 'ok' | 'error' | 'warning' | 'loading'
  text?: string
}>(), {
  status: 'loading',
})

const badgeClass = computed(() => `status-badge--${props.status}`)

const badgeText = computed(() => {
  switch (props.status) {
    case 'ok': return 'ОК'
    case 'error': return 'Ошибка'
    case 'warning': return '—'
    case 'loading': return '...'
  }
})
</script>

<style scoped>
.status-badge {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 3px 10px;
  border-radius: 12px;
  font-size: 0.78rem;
  font-weight: 500;
  user-select: none;
  white-space: nowrap;
}

.status-badge__dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  flex-shrink: 0;
}

.status-badge__label {
  font-weight: 600;
}

.status-badge--ok {
  background: var(--success-bg);
  color: var(--success);
}
.status-badge--ok .status-badge__dot {
  background: var(--success);
  box-shadow: 0 0 6px var(--success);
}

.status-badge--error {
  background: var(--danger-bg);
  color: var(--danger);
}
.status-badge--error .status-badge__dot {
  background: var(--danger);
  box-shadow: 0 0 6px var(--danger);
}

.status-badge--warning {
  background: var(--warning-bg);
  color: var(--warning);
}
.status-badge--warning .status-badge__dot {
  background: var(--warning);
  box-shadow: 0 0 6px var(--warning);
}

.status-badge--loading {
  background: var(--warning-bg);
  color: var(--warning);
}
.status-badge--loading .status-badge__dot {
  background: var(--warning);
  animation: pulse 1.5s ease-in-out infinite;
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.3; }
}
</style>

<template>
  <aside class="sidebar">
    <div class="sidebar__brand">
      <div class="sidebar__logo">KH</div>
      <div class="sidebar__title">KatHub</div>
    </div>

    <nav class="sidebar__nav">
      <router-link to="/" class="sidebar__link" active-class="sidebar__link--active">
        <svg class="sidebar__icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <rect x="3" y="3" width="7" height="7" rx="1"/>
          <rect x="14" y="3" width="7" height="7" rx="1"/>
          <rect x="3" y="14" width="7" height="7" rx="1"/>
          <rect x="14" y="14" width="7" height="7" rx="1"/>
        </svg>
        <span>Панель</span>
      </router-link>

      <router-link to="/settings" class="sidebar__link" active-class="sidebar__link--active">
        <svg class="sidebar__icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <circle cx="12" cy="12" r="3"/>
          <path d="M12 1v2m0 18v2M4.22 4.22l1.42 1.42m12.72 12.72 1.42 1.42M1 12h2m18 0h2M4.22 19.78l1.42-1.42M18.36 5.64l1.42-1.42"/>
        </svg>
        <span>Настройки</span>
      </router-link>
    </nav>

    <div class="sidebar__footer">
      <div class="sidebar__status">
        <StatusBadge :status="serverOk ? 'ok' : 'error'">
          {{ serverOk ? 'Сервер онлайн' : 'Сервер офлайн' }}
        </StatusBadge>
      </div>
      <div class="sidebar__version" v-if="version">v{{ version }}</div>
    </div>
  </aside>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { useAppStore } from '@/stores/appStore'
import StatusBadge from './StatusBadge.vue'

const store = useAppStore()
const serverOk = computed(() => store.serverStatus?.status === 'ok')
const version = computed(() => store.serverStatus?.version ?? null)
</script>

<style scoped>
.sidebar {
  display: flex;
  flex-direction: column;
  width: var(--sidebar-width);
  height: 100%;
  background: var(--bg-secondary);
  border-right: 1px solid var(--border-color);
  flex-shrink: 0;
  user-select: none;
}

.sidebar__brand {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 20px 18px;
  border-bottom: 1px solid var(--border-color);
}

.sidebar__logo {
  width: 36px;
  height: 36px;
  display: flex;
  align-items: center;
  justify-content: center;
  background: linear-gradient(135deg, var(--accent), var(--accent-dark));
  color: #fff;
  border-radius: var(--radius);
  font-weight: 700;
  font-size: 0.85rem;
  letter-spacing: 0.5px;
}

.sidebar__title {
  font-size: 1.1rem;
  font-weight: 600;
  color: var(--text-primary);
}

.sidebar__nav {
  flex: 1;
  padding: 12px 10px;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.sidebar__link {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 10px 12px;
  border-radius: var(--radius);
  color: var(--text-secondary);
  font-size: 0.9rem;
  text-decoration: none;
  transition: all 0.15s ease;
}

.sidebar__link:hover {
  background: var(--bg-hover);
  color: var(--text-primary);
}

.sidebar__link--active {
  background: rgba(139, 92, 246, 0.12);
  color: var(--accent-light);
}

.sidebar__icon {
  width: 18px;
  height: 18px;
  flex-shrink: 0;
}

.sidebar__footer {
  padding: 14px 18px;
  border-top: 1px solid var(--border-color);
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.sidebar__status {
  display: flex;
}

.sidebar__version {
  font-family: var(--font-mono);
  font-size: 0.7rem;
  color: var(--text-muted);
}
</style>

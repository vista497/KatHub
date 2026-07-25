<template>
  <div class="dashboard">
    <h1 class="dashboard__title">Панель управления</h1>

    <div v-if="store.loading" class="dashboard__loading">
      Загрузка...
    </div>

    <div v-else-if="store.error" class="dashboard__error card">
      <StatusBadge status="error">Ошибка подключения</StatusBadge>
      <p class="dashboard__error-text">{{ store.error }}</p>
      <button class="dashboard__retry-btn" @click="store.fetchStatus()">Повторить</button>
    </div>

    <template v-else>
      <!-- Server Status Card -->
      <section class="card dashboard__section">
        <h2 class="dashboard__section-title">Сервер</h2>
        <div class="dashboard__grid">
          <div class="dashboard__item">
            <span class="dashboard__label">Статус</span>
            <StatusBadge :status="serverOk ? 'ok' : 'error'">
              {{ serverOk ? 'ОК' : store.serverStatus?.status ?? '—' }}
            </StatusBadge>
          </div>
          <div class="dashboard__item">
            <span class="dashboard__label">Версия</span>
            <span class="dashboard__value mono">{{ store.serverStatus?.version ?? '—' }}</span>
          </div>
          <div class="dashboard__item">
            <span class="dashboard__label">Аптайм</span>
            <span class="dashboard__value mono">{{ uptime }}</span>
          </div>
        </div>
      </section>

      <!-- WebSocket Status Card -->
      <section class="card dashboard__section">
        <h2 class="dashboard__section-title">WebSocket</h2>
        <div class="dashboard__grid">
          <div class="dashboard__item">
            <span class="dashboard__label">Статус</span>
            <StatusBadge :status="wsOk ? 'ok' : 'error'">
              {{ wsOk ? 'ОК' : store.wsStatus?.status ?? '—' }}
            </StatusBadge>
          </div>
          <div class="dashboard__item">
            <span class="dashboard__label">Порт</span>
            <span class="dashboard__value mono">{{ store.wsStatus?.port ?? '—' }}</span>
          </div>
          <div class="dashboard__item">
            <span class="dashboard__label">Клиенты</span>
            <span class="dashboard__value mono">{{ store.wsStatus?.clients ?? 0 }}</span>
          </div>
        </div>
        <div v-if="store.wsStatus?.subscriptions?.length" class="dashboard__subscriptions">
          <span class="dashboard__label">Подписки</span>
          <div class="dashboard__tags">
            <span
              v-for="sub in store.wsStatus.subscriptions"
              :key="sub"
              class="dashboard__tag"
            >{{ sub }}</span>
          </div>
        </div>
      </section>

      <!-- Refresh -->
      <button class="dashboard__refresh-btn" @click="store.fetchStatus()">
        Обновить
      </button>
    </template>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { useAppStore } from '@/stores/appStore'
import StatusBadge from '@/components/StatusBadge.vue'

const store = useAppStore()

const serverOk = computed(() => store.serverStatus?.status === 'ok')
const wsOk = computed(() => store.wsStatus?.status === 'ok')

const uptime = computed(() => {
  const seconds = store.serverStatus?.uptime ?? 0
  if (seconds < 60) return `${seconds} с`
  if (seconds < 3600) return `${Math.floor(seconds / 60)} мин`
  if (seconds < 86400) return `${Math.floor(seconds / 3600)} ч ${Math.floor((seconds % 3600) / 60)} мин`
  const d = Math.floor(seconds / 86400)
  const h = Math.floor((seconds % 86400) / 3600)
  return `${d} д ${h} ч`
})
</script>

<style scoped>
.dashboard {
  display: flex;
  flex-direction: column;
  gap: 20px;
  max-width: 700px;
}

.dashboard__title {
  font-size: 1.5rem;
  font-weight: 600;
  color: var(--text-primary);
  margin: 0;
}

.dashboard__loading {
  color: var(--text-secondary);
  font-style: italic;
}

.dashboard__error {
  display: flex;
  flex-direction: column;
  gap: 12px;
  align-items: flex-start;
}

.dashboard__error-text {
  color: var(--text-secondary);
  font-size: 0.9rem;
}

.dashboard__retry-btn,
.dashboard__refresh-btn {
  padding: 8px 20px;
  border: 1px solid var(--accent);
  border-radius: var(--radius);
  background: rgba(139, 92, 246, 0.1);
  color: var(--accent-light);
  font-size: 0.9rem;
  cursor: pointer;
  transition: all 0.15s;
}

.dashboard__retry-btn:hover,
.dashboard__refresh-btn:hover {
  background: rgba(139, 92, 246, 0.2);
}

.dashboard__refresh-btn {
  align-self: flex-start;
}

.dashboard__section {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.dashboard__section-title {
  font-size: 1rem;
  font-weight: 600;
  color: var(--text-primary);
  padding-bottom: 8px;
  border-bottom: 1px solid var(--border-color);
}

.dashboard__grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(180px, 1fr));
  gap: 16px;
}

.dashboard__item {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.dashboard__label {
  font-size: 0.78rem;
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.dashboard__value {
  font-size: 1rem;
  color: var(--text-primary);
}

.dashboard__subscriptions {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding-top: 8px;
  border-top: 1px solid var(--border-color);
}

.dashboard__tags {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}

.dashboard__tag {
  display: inline-block;
  padding: 2px 10px;
  background: rgba(139, 92, 246, 0.12);
  color: var(--accent-light);
  border-radius: 12px;
  font-family: var(--font-mono);
  font-size: 0.75rem;
}
</style>

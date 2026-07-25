<template>
  <div class="dashboard">
    <h1 class="dashboard__title">Панель управления</h1>

    <div v-if="serverStore.loading && !serverStore.serverStatus" class="dashboard__loading">
      Загрузка...
    </div>

    <div v-else-if="serverStore.error" class="dashboard__error card">
      <StatusBadge status="error">Ошибка</StatusBadge>
      <p class="dashboard__error-text">{{ serverStore.error }}</p>
      <button class="dashboard__retry-btn" @click="serverStore.fetchStatus()">Повторить</button>
    </div>

    <template v-else>
      <!-- Server Status Card -->
      <section class="card dashboard__section">
        <h2 class="dashboard__section-title">Сервер</h2>
        <StatusBadge :status="serverStore.loading ? 'loading' : serverStore.serverStatus?.status === 'ok' ? 'ok' : 'error'">
          {{ serverStore.loading ? 'Загрузка...' : serverStore.serverStatus?.status === 'ok' ? 'ОК' : (serverStore.serverStatus?.status ?? '—') }}
        </StatusBadge>
        <div class="dashboard__grid" v-if="serverStore.serverStatus">
          <div class="dashboard__item">
            <span class="dashboard__label">Версия</span>
            <span class="dashboard__value mono">{{ serverStore.serverStatus.version }}</span>
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
            <span class="dashboard__ws-indicator" :class="wsStatusClass">
              <span class="dashboard__ws-dot"></span>
              {{ wsStatusLabel }}
            </span>
          </div>
        </div>
      </section>

      <!-- Plugins Card -->
      <section class="card dashboard__section">
        <h2 class="dashboard__section-title">Плагины</h2>
        <div v-if="pluginsStore.loading" class="dashboard__loading">Загрузка...</div>
        <div v-else-if="pluginsStore.error" class="dashboard__plugin-error">
          <span class="dashboard__error-text">Ошибка: {{ pluginsStore.error }}</span>
          <button class="dashboard__retry-btn" @click="pluginsStore.fetchPlugins()">Повторить</button>
        </div>
        <div v-else-if="pluginsStore.plugins.length === 0" class="dashboard__empty">
          Нет установленных плагинов
        </div>
        <div v-else class="dashboard__plugins-list">
          <div
            v-for="plugin in pluginsStore.plugins"
            :key="plugin.name"
            class="dashboard__plugin-item"
          >
            <span class="dashboard__plugin-dot" :class="plugin.enabled ? 'dashboard__plugin-dot--on' : 'dashboard__plugin-dot--off'"></span>
            <span class="dashboard__plugin-name">{{ plugin.name }}</span>
            <span class="dashboard__plugin-version mono">v{{ plugin.version }}</span>
            <span v-if="plugin.description" class="dashboard__plugin-desc">{{ plugin.description }}</span>
          </div>
        </div>
      </section>

      <!-- Event Log -->
      <section class="card dashboard__section">
        <h2 class="dashboard__section-title">События</h2>
        <div v-if="eventsStore.events.length === 0" class="dashboard__empty">
          Нет событий
        </div>
        <div v-else class="dashboard__events-list">
          <div
            v-for="event in eventsStore.events.slice(0, 20)"
            :key="event.id"
            class="dashboard__event"
            :class="`dashboard__event--${event.type}`"
          >
            <span class="dashboard__event-type">{{ event.type.toUpperCase() }}</span>
            <span class="dashboard__event-msg">{{ event.message }}</span>
            <span class="dashboard__event-time mono">{{ formatTime(event.timestamp) }}</span>
          </div>
        </div>
        <button
          v-if="eventsStore.events.length > 0"
          class="dashboard__clear-btn"
          @click="eventsStore.clearEvents()"
        >Очистить</button>
      </section>

      <!-- Refresh -->
      <button class="dashboard__refresh-btn" @click="serverStore.fetchStatus()">
        Обновить
      </button>
    </template>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted } from 'vue'
import { useServerStore } from '@/stores/serverStore'
import { usePluginsStore } from '@/stores/pluginsStore'
import { useEventsStore } from '@/stores/eventsStore'
import StatusBadge from '@/components/StatusBadge.vue'

const serverStore = useServerStore()
const pluginsStore = usePluginsStore()
const eventsStore = useEventsStore()

const wsStatusClass = computed(() => {
  if (serverStore.wsConnectionStatus === 'connected') return 'dashboard__ws-indicator--ok'
  if (serverStore.wsConnectionStatus === 'reconnecting') return 'dashboard__ws-indicator--reconnecting'
  return 'dashboard__ws-indicator--error'
})

const wsStatusLabel = computed(() => {
  switch (serverStore.wsConnectionStatus) {
    case 'connected': return 'Подключён'
    case 'reconnecting': return 'Переподключение...'
    default: return 'Отключён'
  }
})

const uptime = computed(() => {
  const seconds = serverStore.serverStatus?.uptime ?? 0
  if (seconds < 60) return `${seconds} с`
  if (seconds < 3600) return `${Math.floor(seconds / 60)} мин`
  if (seconds < 86400) return `${Math.floor(seconds / 3600)} ч ${Math.floor((seconds % 3600) / 60)} мин`
  const d = Math.floor(seconds / 86400)
  const h = Math.floor((seconds % 86400) / 3600)
  return `${d} д ${h} ч`
})

function formatTime(ts: number): string {
  const d = new Date(ts)
  return d.toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
}

onMounted(() => {
  serverStore.fetchStatus()
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

.dashboard__empty {
  color: var(--text-muted);
  font-size: 0.9rem;
  padding: 12px 0;
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
.dashboard__refresh-btn,
.dashboard__clear-btn {
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
.dashboard__refresh-btn:hover,
.dashboard__clear-btn:hover {
  background: rgba(139, 92, 246, 0.2);
}

.dashboard__clear-btn {
  align-self: flex-start;
  border-color: var(--border-color);
  color: var(--text-muted);
  font-size: 0.8rem;
  padding: 5px 14px;
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

/* WS inline indicator */
.dashboard__ws-indicator {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 0.85rem;
  font-weight: 500;
}

.dashboard__ws-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  flex-shrink: 0;
}

.dashboard__ws-indicator--ok {
  color: var(--success);
}
.dashboard__ws-indicator--ok .dashboard__ws-dot {
  background: var(--success);
  box-shadow: 0 0 6px var(--success);
}

.dashboard__ws-indicator--error {
  color: var(--danger);
}
.dashboard__ws-indicator--error .dashboard__ws-dot {
  background: var(--danger);
  box-shadow: 0 0 6px var(--danger);
}

.dashboard__ws-indicator--reconnecting {
  color: var(--warning);
}
.dashboard__ws-indicator--reconnecting .dashboard__ws-dot {
  background: var(--warning);
  animation: pulse 1.5s ease-in-out infinite;
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.3; }
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

/* Plugins */
.dashboard__plugin-error {
  display: flex;
  flex-direction: column;
  gap: 8px;
  align-items: flex-start;
}

.dashboard__plugins-list {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.dashboard__plugin-item {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 12px;
  background: var(--bg-secondary);
  border-radius: var(--radius);
  flex-wrap: wrap;
}

.dashboard__plugin-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  flex-shrink: 0;
}

.dashboard__plugin-dot--on {
  background: var(--success);
  box-shadow: 0 0 4px var(--success);
}

.dashboard__plugin-dot--off {
  background: var(--text-muted);
}

.dashboard__plugin-name {
  font-weight: 600;
  font-size: 0.9rem;
  color: var(--text-primary);
}

.dashboard__plugin-version {
  font-size: 0.75rem;
  color: var(--text-muted);
}

.dashboard__plugin-desc {
  width: 100%;
  font-size: 0.78rem;
  color: var(--text-secondary);
  padding-left: 16px;
}

/* Events */
.dashboard__events-list {
  display: flex;
  flex-direction: column;
  gap: 2px;
  max-height: 320px;
  overflow-y: auto;
}

.dashboard__event {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 5px 10px;
  border-radius: 4px;
  font-size: 0.82rem;
}

.dashboard__event:nth-child(odd) {
  background: rgba(255, 255, 255, 0.02);
}

.dashboard__event-type {
  font-size: 0.65rem;
  font-weight: 700;
  padding: 1px 6px;
  border-radius: 3px;
  flex-shrink: 0;
  min-width: 42px;
  text-align: center;
}

.dashboard__event--info .dashboard__event-type {
  background: rgba(59, 130, 246, 0.2);
  color: #60a5fa;
}
.dashboard__event--warning .dashboard__event-type {
  background: rgba(251, 191, 36, 0.2);
  color: #fbbf24;
}
.dashboard__event--error .dashboard__event-type {
  background: rgba(239, 68, 68, 0.2);
  color: #f87171;
}
.dashboard__event--debug .dashboard__event-type {
  background: rgba(156, 163, 175, 0.2);
  color: #9ca3af;
}

.dashboard__event-msg {
  flex: 1;
  color: var(--text-primary);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.dashboard__event-time {
  font-size: 0.7rem;
  color: var(--text-muted);
  flex-shrink: 0;
}
</style>

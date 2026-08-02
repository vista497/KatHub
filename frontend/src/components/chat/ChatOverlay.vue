<script setup lang="ts">
import { useChatStore, isOwnChatSession, sessionLabel } from '../../stores/chatStore'
import { ref, computed, watch, onUnmounted } from 'vue'
import ChatPanel from './ChatPanel.vue'
import RawLog from './RawLog.vue'

const chat = useChatStore()
const activeTab = ref<'chat' | 'raw'>('chat')

const ownSessions = computed(() => chat.sessions.filter(isOwnChatSession))
const otherSessions = computed(() => chat.sessions.filter(s => !isOwnChatSession(s)))

// Resize state — persist panel width
const LS_PANEL_WIDTH = 'kathub-panel-width'
function loadPanelWidth(): number {
  try {
    const v = localStorage.getItem(LS_PANEL_WIDTH)
    if (v) return parseInt(v, 10) || 420
  } catch { /* ignore */ }
  return 420
}
function savePanelWidth(w: number) {
  try { localStorage.setItem(LS_PANEL_WIDTH, String(w)) } catch { /* ignore */ }
}

const panelWidth = ref(loadPanelWidth())
watch(panelWidth, savePanelWidth)
const isResizing = ref(false)
const windowWidth = ref(window.innerWidth)

// When panel exceeds 1/3 of screen, center it
const isCentered = computed(() => panelWidth.value > windowWidth.value / 3)

function onWindowResize() {
  windowWidth.value = window.innerWidth
}
window.addEventListener('resize', onWindowResize)
onUnmounted(() => {
  window.removeEventListener('resize', onWindowResize)
})

function startResize(e: MouseEvent) {
  isResizing.value = true
  const startX = e.clientX
  const startW = panelWidth.value

  function onMove(ev: MouseEvent) {
    if (!isResizing.value) return
    const delta = startX - ev.clientX
    panelWidth.value = Math.max(280, startW + delta)  // no max
  }

  function onUp() {
    isResizing.value = false
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
    document.body.style.cursor = ''
    document.body.style.userSelect = ''
  }

  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
  document.body.style.cursor = 'col-resize'
  document.body.style.userSelect = 'none'
}

onUnmounted(() => {
  document.body.style.cursor = ''
  document.body.style.userSelect = ''
})
</script>

<template>
  <div class="chat-overlay" :class="{ open: chat.panelOpen }">
    <!-- Toggle button (visible when panel closed) -->
    <button v-if="!chat.panelOpen" class="toggle-btn" @click="chat.panelOpen = true">
      💬
    </button>

    <!-- Slide-out panel -->
    <div class="chat-panel-container"
      :class="{ resizing: isResizing, centered: isCentered }"
      :style="{ width: panelWidth + 'px' }">
      <!-- Resize handle -->
      <div class="resize-handle" @mousedown="startResize"></div>

      <!-- Sessions sidebar (collapsible) -->
      <div class="sessions-sidebar" :class="{ hidden: !chat.sessionsVisible }">
        <div class="sessions-header">
          <span class="sessions-title">Чаты</span>
          <button class="hide-sessions-btn" @click="chat.toggleSessions()" title="Hide sessions list">
            ◀
          </button>
        </div>

        <button class="new-chat-btn" @click="chat.newSession()">
          + New Chat
        </button>

        <div class="sessions-list">
          <div v-if="ownSessions.length" class="session-group-header">Мои чаты</div>
          <div
            v-for="s in ownSessions"
            :key="s.id"
            class="session-item"
            :class="{ active: chat.activeSessionId === s.id }"
            @click="chat.openSession(s.id)"
          >
            <span class="session-title">{{ s.title }}</span>
            <button
              class="session-delete"
              @click.stop="chat.deleteSession(s.id)"
              title="Delete session"
            >✕</button>
          </div>
          <div v-if="otherSessions.length" class="session-group-header">Другие (только просмотр)</div>
          <div
            v-for="s in otherSessions"
            :key="s.id"
            class="session-item readonly"
            :class="{ active: chat.activeSessionId === s.id }"
            @click="chat.openSession(s.id)"
          >
            <span class="session-title">{{ s.title }}</span>
          </div>
          <div v-if="chat.sessions.length === 0" class="no-sessions">
            No chats yet
          </div>
        </div>
      </div>

      <!-- Sessions toggle when hidden -->
      <button
        v-if="!chat.sessionsVisible"
        class="show-sessions-btn"
        @click="chat.toggleSessions()"
        title="Show sessions list"
      >
        ▶
      </button>

      <!-- Chat panel -->
      <div class="chat-main">
        <div class="chat-topbar">
          <span class="chat-session-name">
            {{ sessionLabel(chat.sessions.find(s => s.id === chat.activeSessionId)) }}
          </span>
          <div class="tab-switcher">
            <button :class="{ active: activeTab === 'chat' }" @click="activeTab = 'chat'">💬</button>
            <button :class="{ active: activeTab === 'raw' }" @click="activeTab = 'raw'; chat.toggleRaw()">📡</button>
          </div>
          <span v-if="chat.sending" class="sending-dot">●</span>
          <button class="close-panel-btn" @click="chat.closePanel()">✕</button>
        </div>
        <RawLog v-if="activeTab === 'raw'" />
        <ChatPanel v-if="activeTab === 'chat'" />
      </div>
    </div>
  </div>
</template>

<style scoped>
.chat-overlay {
  position: relative;
  height: 100vh;
  display: flex;
  align-items: stretch;
}

/* Toggle button — visible when panel is closed */
.toggle-btn {
  position: absolute;
  right: 8px;
  top: 50%;
  transform: translateY(-50%);
  width: 36px;
  height: 36px;
  border-radius: 8px 0 0 8px;
  border: 1px solid rgba(124, 92, 255, 0.2);
  background: rgba(124, 92, 255, 0.12);
  color: #ccccee;
  cursor: pointer;
  font-size: 16px;
  z-index: 50;
  transition: all 0.2s;
}
.toggle-btn:hover {
  background: rgba(124, 92, 255, 0.3);
}

/* Slide-out panel container */
.chat-panel-container {
  position: fixed;
  right: 0;
  top: 0;
  bottom: 0;
  width: 480px;
  max-width: 100vw;
  background: #0a0a14;
  border-left: 1px solid rgba(124, 92, 255, 0.15);
  display: flex;
  flex-direction: row;
  transform: translateX(calc(100% + 4px));
  transition: transform 0.25s ease;
  z-index: 100;
}

.chat-overlay.open .chat-panel-container {
  transform: translateX(0);
}

/* Centered panel uses opacity animation instead of slide */
.chat-panel-container.centered {
  right: auto;
  left: 50%;
  border-right: 1px solid rgba(124, 92, 255, 0.15);
  border-radius: 8px;
  box-shadow: 0 0 60px rgba(124, 92, 255, 0.15);
  transform: translateX(-50%) scale(0.95);
  opacity: 0;
  transition: opacity 0.3s ease, transform 0.3s ease;
}
.chat-overlay.open .chat-panel-container.centered {
  transform: translateX(-50%) scale(1);
  opacity: 1;
}

.chat-panel-container.resizing {
  transition: none;
}

/* Resize handle — left edge of panel */
.resize-handle {
  position: absolute;
  left: -4px;
  top: 0;
  bottom: 0;
  width: 8px;
  cursor: col-resize;
  z-index: 101;
  background: transparent;
  transition: background 0.15s;
}
.resize-handle:hover {
  background: rgba(124, 92, 255, 0.25);
}

/* Sessions sidebar */
.sessions-sidebar {
  width: 160px;
  flex-shrink: 0;
  display: flex;
  flex-direction: column;
  border-right: 1px solid rgba(124, 92, 255, 0.08);
  overflow: hidden;
  transition: width 0.2s, opacity 0.2s;
}

.sessions-sidebar.hidden {
  width: 0;
  opacity: 0;
  overflow: hidden;
}

.sessions-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 8px 10px;
  border-bottom: 1px solid rgba(124, 92, 255, 0.06);
}

.sessions-title {
  font-size: 11px;
  color: #8888aa;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.hide-sessions-btn {
  background: none;
  border: none;
  color: #666888;
  cursor: pointer;
  font-size: 10px;
  padding: 2px;
}
.hide-sessions-btn:hover { color: #ccccee; }

.new-chat-btn {
  margin: 6px;
  padding: 6px;
  background: rgba(124, 92, 255, 0.15);
  border: 1px solid rgba(124, 92, 255, 0.2);
  border-radius: 5px;
  color: #ccccee;
  cursor: pointer;
  font-size: 11px;
  transition: background 0.15s;
  text-align: center;
}
.new-chat-btn:hover {
  background: rgba(124, 92, 255, 0.3);
}

.sessions-list {
  flex: 1;
  overflow-y: auto;
  padding: 2px 4px;
}

.session-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 6px 8px;
  border-radius: 4px;
  cursor: pointer;
  transition: background 0.1s;
  margin-bottom: 1px;
}
.session-item:hover {
  background: rgba(124, 92, 255, 0.1);
}
.session-item.active {
  background: rgba(124, 92, 255, 0.2);
}

/* Read-only (чужие сессии: telegram/cli/cron/run_*) */
.session-item.readonly {
  cursor: default;
}
.session-item.readonly .session-title {
  color: #666888;
  font-style: italic;
}
.session-item.readonly.active .session-title {
  color: #aaaacc;
}

.session-title {
  font-size: 11px;
  color: #aaaacc;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  flex: 1;
}
.session-item.active .session-title {
  color: #fff;
}

.session-delete {
  background: none;
  border: none;
  color: #666888;
  cursor: pointer;
  font-size: 10px;
  padding: 0 4px;
  opacity: 0;
  transition: opacity 0.1s;
}
.session-item:hover .session-delete {
  opacity: 1;
}
.session-delete:hover {
  color: #ff6b6b;
}

.no-sessions {
  font-size: 11px;
  color: #666888;
  padding: 8px;
  text-align: center;
}

/* Show sessions button (when hidden) */
.show-sessions-btn {
  position: absolute;
  left: 0;
  top: 50%;
  transform: translateY(-50%);
  background: rgba(124, 92, 255, 0.15);
  border: 1px solid rgba(124, 92, 255, 0.2);
  border-radius: 0 4px 4px 0;
  color: #8888aa;
  cursor: pointer;
  font-size: 10px;
  padding: 8px 4px;
  z-index: 5;
  transition: all 0.15s;
}
.show-sessions-btn:hover {
  background: rgba(124, 92, 255, 0.3);
  color: #ccccee;
}

/* Main chat area */
.chat-main {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-width: 0;
  min-height: 0;
}

.chat-topbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 8px 12px;
  border-bottom: 1px solid rgba(124, 92, 255, 0.08);
  flex-shrink: 0;
}

.chat-session-name {
  font-size: 12px;
  color: #8888aa;
  font-weight: 600;
}

.sending-dot {
  color: #7c5cff;
  font-size: 8px;
  animation: pulse 1s infinite;
}

@keyframes pulse {
  0%, 100% { opacity: 0.3; }
  50% { opacity: 1; }
}

.close-panel-btn {
  background: none;
  border: none;
  color: #666888;
  cursor: pointer;
  font-size: 14px;
  padding: 2px 6px;
  border-radius: 4px;
  transition: all 0.15s;
}
.close-panel-btn:hover {
  color: #fff;
  background: rgba(255, 255, 255, 0.08);
}

.tab-switcher {
  display: flex;
  gap: 1px;
  background: rgba(255,255,255,0.04);
  border-radius: 4px;
  padding: 1px;
}
.tab-switcher button {
  background: none;
  border: none;
  color: #666;
  font-size: 12px;
  padding: 2px 6px;
  border-radius: 3px;
  cursor: pointer;
  transition: all 0.12s;
}
.tab-switcher button.active {
  background: rgba(124, 92, 255, 0.2);
  color: #ccc;
}
.tab-switcher button:hover:not(.active) {
  color: #999;
}
</style>

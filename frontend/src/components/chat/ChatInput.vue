<script setup lang="ts">
import { ref, computed } from 'vue'

const props = defineProps<{ sending?: boolean; readOnly?: boolean }>()
const emit = defineEmits<{ send: [text: string] }>()
const text = ref('')
const textarea = ref<HTMLTextAreaElement>()
const showCommands = ref(false)
const selectedCmd = ref(0)

// Available commands (matching Hermes Telegram bot)
const COMMANDS = [
  { cmd: '/help', desc: 'Помощь' },
  { cmd: '/status', desc: 'Статус системы' },
  { cmd: '/model', desc: 'Текущая модель' },
  { cmd: '/config', desc: 'Конфигурация' },
  { cmd: '/restart', desc: 'Перезапуск Hermes' },
  { cmd: '/update', desc: 'Обновить Hermes' },
  { cmd: '/memory', desc: 'Статистика памяти' },
  { cmd: '/sessions', desc: 'Активные сессии' },
  { cmd: '/clear', desc: 'Очистить контекст' },
  { cmd: '/cost', desc: 'Расходы' },
  { cmd: '/health', desc: 'Проверка здоровья' },
  { cmd: '/stop', desc: 'Остановить генерацию' },
]

const filteredCommands = computed(() => {
  const t = text.value.trim().toLowerCase()
  if (!t.startsWith('/')) return []
  return COMMANDS.filter(c => c.cmd.toLowerCase().startsWith(t))
})

function handleSend() {
  const trimmed = text.value.trim()
  if (!trimmed) return
  emit('send', trimmed)
  text.value = ''
  showCommands.value = false
  if (textarea.value) textarea.value.style.height = 'auto'
}

function selectCommand(cmd: string) {
  text.value = cmd + ' '
  showCommands.value = false
  textarea.value?.focus()
}

function onKeydown(e: KeyboardEvent) {
  if (showCommands.value && filteredCommands.value.length > 0) {
    if (e.key === 'ArrowDown') {
      e.preventDefault()
      selectedCmd.value = Math.min(selectedCmd.value + 1, filteredCommands.value.length - 1)
      return
    }
    if (e.key === 'ArrowUp') {
      e.preventDefault()
      selectedCmd.value = Math.max(selectedCmd.value - 1, 0)
      return
    }
    if (e.key === 'Enter' || e.key === 'Tab') {
      e.preventDefault()
      selectCommand(filteredCommands.value[selectedCmd.value].cmd)
      return
    }
    if (e.key === 'Escape') {
      showCommands.value = false
      return
    }
  }
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault()
    handleSend()
  }
}

function onInput() {
  autoResize()
  const t = text.value.trim()
  showCommands.value = t.startsWith('/') && t.length >= 1
  selectedCmd.value = 0
}

function autoResize() {
  if (!textarea.value) return
  textarea.value.style.height = 'auto'
  textarea.value.style.height = textarea.value.scrollHeight + 'px'
}

// ── Speech capture (текст из речи) ─────────────────────────────
const SPEECH_CAPTURE_SECONDS = 10
const speech = ref<{
  loading: boolean
  text: string
  bufferSeconds: number
  connected: boolean
  error: string
}>({ loading: false, text: '', bufferSeconds: 0, connected: false, error: '' })

async function captureSpeech() {
  if (speech.value.loading) return
  speech.value = { loading: true, text: '', bufferSeconds: 0, connected: false, error: '' }
  try {
    const resp = await fetch('/api/speech', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command: 'capture', seconds: SPEECH_CAPTURE_SECONDS }),
    })
    const data = await resp.json()
    if (!resp.ok) {
      speech.value.error = data.error || ('HTTP ' + resp.status)
      return
    }
    speech.value.text = data.text || ''
    speech.value.bufferSeconds = data.bufferSeconds || 0
    speech.value.connected = !!data.connected
    if (data.connected === false) {
      speech.value.error = 'STT-сервер не запущен'
    }
  } catch (e) {
    speech.value.error = 'Ошибка: ' + String(e)
  } finally {
    speech.value.loading = false
  }
}

function clearSpeech() {
  speech.value = { loading: false, text: '', bufferSeconds: 0, connected: false, error: '' }
}

function insertSpeechText() {
  if (!speech.value.text) return
  text.value = speech.value.text
  clearSpeech()
  autoResize()
  textarea.value?.focus()
}
</script>

<template>
  <div class="chat-input">
    <!-- Commands dropdown -->
    <div v-if="showCommands && filteredCommands.length > 0" class="commands-dropdown">
      <div
        v-for="(c, i) in filteredCommands"
        :key="c.cmd"
        class="command-item"
        :class="{ selected: i === selectedCmd }"
        @mousedown.prevent="selectCommand(c.cmd)"
      >
        <span class="cmd-name">{{ c.cmd }}</span>
        <span class="cmd-desc">{{ c.desc }}</span>
      </div>
    </div>

    <textarea
      ref="textarea"
      v-model="text"
      :disabled="props.readOnly"
      placeholder="Message or /command..."
      rows="1"
      @keydown="onKeydown"
      @input="onInput"
    ></textarea>

    <!-- Speech capture: блок с текстом из речи -->
    <button
      v-if="speech.loading || speech.text || speech.error"
      class="mic-btn active"
      :disabled="speech.loading"
      @click="speech.loading ? undefined : clearSpeech()"
      title="Сбросить распознанный текст"
    >🎤</button>
    <button
      v-else
      class="mic-btn"
      :disabled="props.readOnly"
      @click="captureSpeech"
      title="Распознать последние {{ SPEECH_CAPTURE_SECONDS }} сек речи (из кольцевого буфера)"
    >🎤</button>

    <button v-if="props.sending" class="stop-btn" @click="emit('send', '/stop')" title="Остановить генерацию">
      ■
    </button>
    <button class="send-btn" @click="handleSend" :disabled="props.readOnly || !text.trim() || props.sending">
      ↑
    </button>

    <!-- Распознанный текст из речи -->
    <div v-if="speech.loading || speech.text || speech.error" class="speech-block">
      <div v-if="speech.loading" class="speech-row">
        <span class="speech-spinner">●</span>
        <span>Распознаю речь…</span>
      </div>
      <div v-else-if="speech.error" class="speech-row speech-error">
        <span>⚠ {{ speech.error }}</span>
        <button class="speech-close" @click="clearSpeech()">✕</button>
      </div>
      <div v-else class="speech-row">
        <span class="speech-label">🎤 {{ speech.text }}</span>
        <button class="speech-use" @click="insertSpeechText()">Вставить в сообщение</button>
        <button class="speech-close" @click="clearSpeech()">✕</button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.chat-input {
  display: flex;
  align-items: flex-end;
  gap: var(--space-2);
  padding: var(--space-3) var(--space-4);
  border-top: 1px solid var(--color-border);
  background: var(--color-bg-secondary);
  position: relative;
}

textarea {
  flex: 1;
  resize: none;
  max-height: 120px;
  padding: var(--space-2) var(--space-3);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-lg);
  background: var(--color-chat-bg);
  color: var(--color-text-primary);
  font-family: var(--font-sans);
  font-size: var(--font-size-sm);
  outline: none;
  line-height: 1.4;
}
textarea:focus { border-color: var(--color-accent); }
textarea::placeholder { color: var(--color-text-muted); }

.send-btn {
  width: 36px; height: 36px;
  border-radius: 50%;
  background: var(--color-accent);
  color: white; border: none;
  cursor: pointer;
  display: flex; align-items: center; justify-content: center;
  font-size: var(--font-size-lg);
  flex-shrink: 0;
  transition: all var(--transition-fast);
}
.send-btn:hover:not(:disabled) { transform: scale(1.1); }
.send-btn:disabled { opacity: 0.4; cursor: default; }

/* ── Stop button ─────────────────────────────── */
.stop-btn {
  width: 36px; height: 36px;
  border-radius: 50%;
  background: #ff4444;
  color: white; border: none;
  cursor: pointer;
  display: flex; align-items: center; justify-content: center;
  font-size: 14px;
  flex-shrink: 0;
  transition: all var(--transition-fast);
  animation: pulse-stop 1.5s infinite;
}
.stop-btn:hover { background: #ff6666; transform: scale(1.1); }

@keyframes pulse-stop {
  0%, 100% { box-shadow: 0 0 0 0 rgba(255, 68, 68, 0.4); }
  50%      { box-shadow: 0 0 0 6px rgba(255, 68, 68, 0); }
}

/* ── Commands dropdown ───────────────────────── */
.commands-dropdown {
  position: absolute;
  bottom: 100%;
  left: var(--space-4);
  right: 52px;
  max-height: 200px;
  overflow-y: auto;
  background: #12122a;
  border: 1px solid rgba(124, 92, 255, 0.25);
  border-radius: 8px 8px 0 0;
  margin-bottom: 2px;
  z-index: 200;
  box-shadow: 0 -4px 20px rgba(0, 0, 0, 0.4);
}

.command-item {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 8px 12px;
  cursor: pointer;
  transition: background 0.1s;
}
.command-item:hover,
.command-item.selected {
  background: rgba(124, 92, 255, 0.15);
}

.cmd-name {
  font-family: 'Fira Code', 'JetBrains Mono', monospace;
  font-size: 12px;
  color: #5ce0ff;
  font-weight: 600;
  white-space: nowrap;
}
.cmd-desc {
  font-size: 11px;
  color: #8888aa;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/* ── Speech capture (текст из речи) ─────────────────────────── */
.mic-btn {
  width: 36px; height: 36px;
  border-radius: 50%;
  background: transparent;
  color: var(--color-text-muted);
  border: 1px solid var(--color-border);
  cursor: pointer;
  display: flex; align-items: center; justify-content: center;
  font-size: 15px;
  flex-shrink: 0;
  transition: all var(--transition-fast);
}
.mic-btn:hover:not(:disabled) {
  border-color: var(--color-accent);
  color: var(--color-accent);
  transform: scale(1.08);
}
.mic-btn.active {
  border-color: var(--color-accent);
  color: var(--color-accent);
  background: rgba(124, 92, 255, 0.12);
}
.mic-btn:disabled { opacity: 0.5; cursor: default; }

.speech-block {
  position: absolute;
  bottom: calc(100% + 8px);
  left: var(--space-4);
  right: var(--space-4);
  z-index: 150;
  background: #12122a;
  border: 1px solid rgba(124, 92, 255, 0.35);
  border-radius: 10px;
  padding: 8px 12px;
  box-shadow: 0 -4px 20px rgba(0, 0, 0, 0.4);
  animation: speech-pop 0.18s ease;
}
@keyframes speech-pop {
  from { opacity: 0; transform: translateY(4px); }
  to   { opacity: 1; transform: translateY(0); }
}
.speech-row {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 12px;
  color: var(--color-text-primary);
  line-height: 1.45;
}
.speech-label {
  flex: 1;
  word-break: break-word;
  max-height: 84px;
  overflow-y: auto;
}
.speech-spinner {
  color: var(--color-accent);
  animation: speech-pulse 1s infinite;
}
@keyframes speech-pulse {
  0%, 100% { opacity: 0.3; }
  50%      { opacity: 1; }
}
.speech-error { color: #ff8888; }
.speech-use {
  flex-shrink: 0;
  padding: 4px 10px;
  border-radius: 6px;
  border: 1px solid var(--color-accent);
  background: rgba(124, 92, 255, 0.15);
  color: var(--color-accent-secondary);
  font-size: 11px;
  cursor: pointer;
  white-space: nowrap;
  transition: background 0.15s;
}
.speech-use:hover { background: rgba(124, 92, 255, 0.3); }
.speech-close {
  flex-shrink: 0;
  width: 22px; height: 22px;
  border-radius: 50%;
  border: none;
  background: transparent;
  color: var(--color-text-muted);
  cursor: pointer;
  font-size: 12px;
  transition: background 0.15s, color 0.15s;
}
.speech-close:hover { background: rgba(255, 68, 68, 0.15); color: #ff8888; }
</style>

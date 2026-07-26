<script setup lang="ts">
import { ref, computed } from 'vue'

const props = defineProps<{ sending?: boolean }>()
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
      placeholder="Message or /command..."
      rows="1"
      @keydown="onKeydown"
      @input="onInput"
    ></textarea>
    <button v-if="props.sending" class="stop-btn" @click="emit('send', '/stop')" title="Stop generation">
      ■
    </button>
    <button class="send-btn" @click="handleSend" :disabled="!text.trim() || props.sending">
      ↑
    </button>
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
</style>

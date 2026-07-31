<template>
  <Teleport to="body">
    <div v-if="chat.pendingApproval" class="approval-backdrop" @click.self="() => {}">
      <div class="approval-card">
        <div class="approval-header">
          <span class="approval-icon">⚠️</span>
          <span class="approval-title">Требуется подтверждение</span>
        </div>

        <p class="approval-desc">
          {{ chat.pendingApproval.description || 'Hermes хочет выполнить команду на этом компьютере.' }}
        </p>

        <div v-if="chat.pendingApproval.command" class="approval-command">
          <code>{{ chat.pendingApproval.command }}</code>
        </div>

        <div class="approval-actions">
          <button
            v-if="hasChoice('once')"
            class="approval-btn approval-btn-once"
            :disabled="chat.approvalBusy"
            @click="resolve('once')"
          >Разрешить один раз</button>
          <button
            v-if="hasChoice('session')"
            class="approval-btn approval-btn-session"
            :disabled="chat.approvalBusy"
            @click="resolve('session')"
          >Разрешить сессию</button>
          <button
            v-if="hasChoice('always')"
            class="approval-btn approval-btn-always"
            :disabled="chat.approvalBusy"
            @click="resolve('always')"
          >Всегда разрешать</button>
          <button
            v-if="hasChoice('deny')"
            class="approval-btn approval-btn-deny"
            :disabled="chat.approvalBusy"
            @click="resolve('deny')"
          >Отклонить</button>
        </div>

        <div v-if="chat.approvalBusy" class="approval-busy">Отправка…</div>
      </div>
    </div>
  </Teleport>
</template>

<script setup lang="ts">
import { useChatStore } from '../../stores/chatStore'

const chat = useChatStore()

function hasChoice(choice: string): boolean {
  return chat.pendingApproval?.choices.includes(choice) ?? false
}

function resolve(choice: string) {
  chat.resolveApproval(choice)
}
</script>

<style scoped>
.approval-backdrop {
  position: fixed;
  inset: 0;
  z-index: 9999;
  background: rgba(0, 0, 0, 0.65);
  backdrop-filter: blur(4px);
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 24px;
}

.approval-card {
  width: min(460px, 100%);
  background: var(--color-bg-secondary, #12122a);
  border: 1px solid var(--color-border, rgba(255, 255, 255, 0.08));
  border-radius: 14px;
  box-shadow: 0 12px 40px rgba(0, 0, 0, 0.5),
              0 0 0 1px var(--color-accent-glow, rgba(124, 92, 255, 0.3));
  padding: 20px 22px;
  color: var(--color-text-primary, #e8e8f0);
}

.approval-header {
  display: flex;
  align-items: center;
  gap: 10px;
  margin-bottom: 12px;
}

.approval-icon {
  font-size: 20px;
}

.approval-title {
  font-size: 15px;
  font-weight: 600;
}

.approval-desc {
  margin: 0 0 14px;
  font-size: 13.5px;
  line-height: 1.5;
  color: var(--color-text-secondary, #8888aa);
  word-break: break-word;
}

.approval-command {
  background: var(--color-bg-tertiary, #1a1a3e);
  border: 1px solid rgba(255, 90, 90, 0.35);
  border-radius: 8px;
  padding: 10px 12px;
  margin-bottom: 18px;
  overflow-x: auto;
}

.approval-command code {
  font-family: 'Cascadia Code', Consolas, monospace;
  font-size: 12.5px;
  color: #ff9d9d;
  white-space: pre-wrap;
  word-break: break-all;
}

.approval-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
}

.approval-btn {
  flex: 1 1 auto;
  min-width: 130px;
  padding: 9px 14px;
  border: 1px solid transparent;
  border-radius: 8px;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
  transition: filter var(--transition-fast, 150ms ease),
              transform var(--transition-fast, 150ms ease);
}

.approval-btn:disabled {
  opacity: 0.5;
  cursor: wait;
}

.approval-btn:not(:disabled):hover {
  filter: brightness(1.12);
}

.approval-btn-once {
  background: var(--color-accent, #7c5cff);
  color: #fff;
}

.approval-btn-session {
  background: var(--color-accent-secondary, #5ce0ff);
  color: #06243a;
}

.approval-btn-always {
  background: #1f7a3d;
  color: #eafff0;
}

.approval-btn-deny {
  background: #a33434;
  color: #ffecec;
}

.approval-busy {
  margin-top: 12px;
  font-size: 12px;
  color: var(--color-text-muted, #555577);
  text-align: center;
}
</style>

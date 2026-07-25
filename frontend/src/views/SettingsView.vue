<template>
  <div class="settings">
    <h1 class="settings__title">Настройки</h1>
    <div class="card settings__placeholder">
      <svg class="settings__icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
        <circle cx="12" cy="12" r="3"/>
        <path d="M12 1v2m0 18v2M4.22 4.22l1.42 1.42m12.72 12.72 1.42 1.42M1 12h2m18 0h2M4.22 19.78l1.42-1.42M18.36 5.64l1.42-1.42"/>
      </svg>
      <p>Настройки приложения появятся здесь в следующих версиях.</p>
    </div>
  </div>
</template>

<script setup lang="ts">
</script>

<style scoped>
.settings {
  display: flex;
  flex-direction: column;
  gap: 20px;
  max-width: 600px;
}

.settings__title {
  font-size: 1.5rem;
  font-weight: 600;
  color: var(--text-primary);
  margin: 0;
}

.settings__placeholder {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 16px;
  padding: 48px 24px;
  text-align: center;
}

.settings__icon {
  width: 48px;
  height: 48px;
  color: var(--text-muted);
  opacity: 0.5;
}

.settings__placeholder p {
  color: var(--text-secondary);
  font-size: 0.95rem;
}
</style>

<template>
  <Sidebar />
  <main class="main-content">
    <router-view />
  </main>
</template>

<script setup lang="ts">
import Sidebar from './components/Sidebar.vue'
</script>

<style scoped>
.main-content {
  flex: 1;
  padding: 28px 32px;
  overflow-y: auto;
  background: var(--bg-primary);
}
</style>

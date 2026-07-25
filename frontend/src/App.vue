<script setup lang="ts">
import { onMounted } from 'vue'
import { useRouter } from 'vue-router'

const router = useRouter()

function isMobile(): boolean {
  // Check touch support + narrow screen
  const hasTouch = 'ontouchstart' in window || navigator.maxTouchPoints > 0
  const narrow = window.innerWidth < 1024
  return hasTouch && narrow
}

onMounted(() => {
  if (isMobile() && router.currentRoute.value.path !== '/mobile') {
    router.replace('/mobile')
  }
})
</script>

<template>
  <router-view />
</template>

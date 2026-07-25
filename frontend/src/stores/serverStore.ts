import { defineStore } from 'pinia'
import { ref } from 'vue'

export type WsConnectionStatus = 'connected' | 'disconnected' | 'reconnecting'

interface ServerStatusData {
  status: string
  version: string
  uptime: number
}

export const useServerStore = defineStore('server', () => {
  const serverStatus = ref<ServerStatusData | null>(null)
  const wsConnectionStatus = ref<WsConnectionStatus>('disconnected')
  const loading = ref(false)
  const error = ref<string | null>(null)

  async function fetchStatus() {
    loading.value = true
    error.value = null
    try {
      const r = await fetch('/api/status')
      if (!r.ok) throw new Error(`HTTP ${r.status}`)
      serverStatus.value = (await r.json()) as ServerStatusData
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    } finally {
      loading.value = false
    }
  }

  return {
    serverStatus,
    wsConnectionStatus,
    loading,
    error,
    fetchStatus,
  }
})

import { defineStore } from 'pinia'
import { ref } from 'vue'

interface ServerStatus {
  status: string
  version: string
  uptime: number
}

interface WsStatus {
  status: string
  port: number
  clients: number
  subscriptions: string[]
}

export const useAppStore = defineStore('app', () => {
  const serverStatus = ref<ServerStatus | null>(null)
  const wsStatus = ref<WsStatus | null>(null)
  const loading = ref(false)
  const error = ref<string | null>(null)
  const title = ref('KatHub')

  async function fetchStatus() {
    loading.value = true
    error.value = null
    try {
      const [server, ws] = await Promise.all([
        fetch('/api/status').then((r) => {
          if (!r.ok) throw new Error(`HTTP ${r.status}`)
          return r.json()
        }),
        fetch('/api/ws/status').then((r) => {
          if (!r.ok) throw new Error(`HTTP ${r.status}`)
          return r.json()
        }),
      ])
      serverStatus.value = server as ServerStatus
      wsStatus.value = ws as WsStatus
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    } finally {
      loading.value = false
    }
  }

  // Auto-fetch on store creation
  fetchStatus()

  return {
    serverStatus,
    wsStatus,
    loading,
    error,
    title,
    fetchStatus,
  }
})

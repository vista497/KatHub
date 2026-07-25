import { defineStore } from 'pinia'
import { ref } from 'vue'

export interface Plugin {
  name: string
  version: string
  enabled: boolean
  description?: string
}

export const usePluginsStore = defineStore('plugins', () => {
  const plugins = ref<Plugin[]>([])
  const loading = ref(false)
  const error = ref<string | null>(null)

  async function fetchPlugins() {
    loading.value = true
    error.value = null
    try {
      const r = await fetch('/api/plugins')
      if (!r.ok) throw new Error(`HTTP ${r.status}`)
      plugins.value = (await r.json()) as Plugin[]
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    } finally {
      loading.value = false
    }
  }

  // Auto-fetch on store creation
  fetchPlugins()

  return {
    plugins,
    loading,
    error,
    fetchPlugins,
  }
})

import { defineStore } from 'pinia'
import { ref } from 'vue'

export interface RawLogEntry {
  id: number
  timestamp: number
  sessionId: string
  raw: any           // original JSON from Hermes API
  direction: 'rx'    // received from Hermes
}

const MAX_ENTRIES = 500

export const useRawLogStore = defineStore('rawLog', () => {
  const entries = ref<RawLogEntry[]>([])
  const enabled = ref(false)

  function pushBatch(sessionId: string, rawMessages: any[]) {
    if (!enabled.value) return
    const now = Date.now()
    for (const m of rawMessages) {
      entries.value.push({
        id: m.id || m.message_id || 0,
        timestamp: now,
        sessionId,
        raw: m,
        direction: 'rx',
      })
    }
    // Keep only last MAX_ENTRIES
    if (entries.value.length > MAX_ENTRIES) {
      entries.value = entries.value.slice(-MAX_ENTRIES)
    }
  }

  function clear() {
    entries.value = []
  }

  function toggle() {
    enabled.value = !enabled.value
  }

  return { entries, enabled, pushBatch, clear, toggle }
})

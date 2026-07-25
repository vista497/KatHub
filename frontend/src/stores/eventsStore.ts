import { defineStore } from 'pinia'
import { ref } from 'vue'

export interface LogEvent {
  id: string
  type: 'info' | 'warning' | 'error' | 'debug'
  message: string
  timestamp: number
}

export const useEventsStore = defineStore('events', () => {
  const events = ref<LogEvent[]>([])
  const maxEvents = 200

  function addEvent(event: Omit<LogEvent, 'id'>) {
    const evt: LogEvent = {
      ...event,
      id: crypto.randomUUID(),
    }
    events.value.unshift(evt)
    if (events.value.length > maxEvents) {
      events.value = events.value.slice(0, maxEvents)
    }
  }

  function clearEvents() {
    events.value = []
  }

  return {
    events,
    addEvent,
    clearEvents,
  }
})

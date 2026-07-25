import { describe, it, expect, beforeEach } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useEventsStore, type LogEvent } from '../../src/stores/eventsStore'

describe('eventsStore', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
  })

  it('initial state: events is empty array', () => {
    const store = useEventsStore()

    expect(store.events).toEqual([])
  })

  it('addEvent adds an event to the beginning of the array', () => {
    const store = useEventsStore()

    store.addEvent({
      type: 'info',
      message: 'Test message',
      timestamp: Date.now(),
    })

    expect(store.events.length).toBe(1)
    expect(store.events[0].type).toBe('info')
    expect(store.events[0].message).toBe('Test message')
    expect(store.events[0].id).toBeDefined()
  })

  it('addEvent prepends — newest event is at index 0', () => {
    const store = useEventsStore()

    store.addEvent({ type: 'info', message: 'First', timestamp: 1 })
    store.addEvent({ type: 'info', message: 'Second', timestamp: 2 })

    expect(store.events[0].message).toBe('Second')
    expect(store.events[1].message).toBe('First')
  })

  it('clearEvents removes all events', () => {
    const store = useEventsStore()

    store.addEvent({ type: 'info', message: 'One', timestamp: 1 })
    store.addEvent({ type: 'error', message: 'Two', timestamp: 2 })

    expect(store.events.length).toBe(2)

    store.clearEvents()

    expect(store.events).toEqual([])
    expect(store.events.length).toBe(0)
  })

  it('limit of 200 events — older events are dropped', () => {
    const store = useEventsStore()

    for (let i = 0; i < 250; i++) {
      store.addEvent({
        type: 'info',
        message: `Event ${i}`,
        timestamp: i,
      })
    }

    expect(store.events.length).toBe(200)
    // Oldest added events (0..49) should be dropped
    // Newest event (249) is at index 0
    expect(store.events[0].message).toBe('Event 249')
    // Last event should be event 50 (the oldest kept)
    expect(store.events[199].message).toBe('Event 50')
  })
})

import { describe, it, expect, beforeEach } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useServerStore } from '../../src/stores/serverStore'

describe('serverStore', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
  })

  it('initial state: serverStatus is null (unknown), loading is false', () => {
    const store = useServerStore()

    expect(store.serverStatus).toBeNull()
    expect(store.loading).toBe(false)
    expect(store.error).toBeNull()
  })

  it('initial wsConnectionStatus is disconnected', () => {
    const store = useServerStore()

    expect(store.wsConnectionStatus).toBe('disconnected')
  })

  it('fetchStatus sets loading to true', () => {
    const store = useServerStore()

    // Don't await — check that loading becomes true synchronously
    store.fetchStatus()
    expect(store.loading).toBe(true)
  })
})

import { describe, it, expect, beforeEach, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'

describe('pluginsStore', () => {
  beforeEach(() => {
    setActivePinia(createPinia())

    // Mock fetch so auto-fetch completes without error
    vi.stubGlobal('fetch', () =>
      Promise.resolve({
        ok: true,
        json: () => Promise.resolve([]),
      } as Response)
    )
  })

  it('initial state: plugins is empty array, loading becomes false after fetch resolves', async () => {
    // Dynamic import so the mock is in place before store creation
    const { usePluginsStore } = await import('../../src/stores/pluginsStore')
    const store = usePluginsStore()

    // Auto-fetch has started, wait for it to resolve
    await vi.waitFor(() => {
      expect(store.loading).toBe(false)
    })

    expect(store.plugins).toEqual([])
    expect(store.error).toBeNull()
  })

  it('plugins ref is an array', async () => {
    const { usePluginsStore } = await import('../../src/stores/pluginsStore')
    const store = usePluginsStore()

    await vi.waitFor(() => {
      expect(store.loading).toBe(false)
    })

    expect(Array.isArray(store.plugins)).toBe(true)
    expect(store.plugins.length).toBe(0)
  })
})

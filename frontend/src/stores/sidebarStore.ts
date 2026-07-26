import { defineStore } from 'pinia'
import { ref, watch } from 'vue'

const LS_KEY = 'kathub-sidebar-state'

function load(): { collapsed: boolean; expandedSections: string[] } | null {
  try {
    const raw = localStorage.getItem(LS_KEY)
    if (raw) return JSON.parse(raw)
  } catch { /* ignore */ }
  return null
}

function save(state: { collapsed: boolean; expandedSections: string[] }) {
  try { localStorage.setItem(LS_KEY, JSON.stringify(state)) } catch { /* ignore */ }
}

export const useSidebarStore = defineStore('sidebar', () => {
  const persisted = load()

  const collapsed = ref(persisted?.collapsed ?? false)
  const expandedSections = ref<Set<string>>(
    new Set(persisted?.expandedSections || ['vault'])
  )

  // Persist on change
  watch([collapsed, expandedSections], () => {
    save({
      collapsed: collapsed.value,
      expandedSections: [...expandedSections.value],
    })
  }, { deep: true })

  function toggle() {
    collapsed.value = !collapsed.value
  }

  function toggleSection(name: string) {
    const next = new Set(expandedSections.value)
    if (next.has(name)) {
      next.delete(name)
    } else {
      next.add(name)
    }
    expandedSections.value = next
  }

  function isExpanded(name: string): boolean {
    return expandedSections.value.has(name)
  }

  return { collapsed, expandedSections, toggle, toggleSection, isExpanded }
})

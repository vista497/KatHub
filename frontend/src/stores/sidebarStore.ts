import { defineStore } from 'pinia'
import { ref } from 'vue'

export const useSidebarStore = defineStore('sidebar', () => {
  const collapsed = ref(false)
  const expandedSections = ref<Set<string>>(new Set(['vault']))

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

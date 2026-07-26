import { defineStore } from 'pinia'
import { ref } from 'vue'

export type Theme = 'dark' | 'light' | 'custom'

export const useThemeStore = defineStore('theme', () => {
  const current = ref<Theme>(
    (localStorage.getItem('kathub-theme') as Theme) || 'dark'
  )

  function apply() {
    document.documentElement.setAttribute('data-theme', current.value)
    localStorage.setItem('kathub-theme', current.value)
  }

  function setTheme(theme: Theme) {
    current.value = theme
    apply()
  }

  function toggle() {
    current.value = current.value === 'dark' ? 'light' : 'dark'
    apply()
  }

  // Apply on init
  apply()

  return { current, setTheme, toggle }
})

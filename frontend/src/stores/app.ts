import { defineStore } from 'pinia'

export const useAppStore = defineStore('app', {
  state: () => ({
    title: 'KatHub',
  }),
  actions: {
    setTitle(newTitle: string) {
      this.title = newTitle
    },
  },
})

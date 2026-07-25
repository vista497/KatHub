import { createRouter, createWebHashHistory } from 'vue-router'

const router = createRouter({
  history: createWebHashHistory(),
  routes: [
    {
      path: '/',
      name: 'home',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/mobile',
      name: 'mobile',
      component: () => import('../views/MobileView.vue'),
    },
    {
      path: '/settings',
      name: 'settings',
      component: () => import('../views/DesktopView.vue'),
    },
  ],
})

export default router

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
    // Settings sub-routes — all render same DesktopView for now
    {
      path: '/settings/plugins',
      name: 'settings-plugins',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/settings/backends',
      name: 'settings-backends',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/settings/theme',
      name: 'settings-theme',
      component: () => import('../views/DesktopView.vue'),
    },
    // Catch-all redirect
    {
      path: '/:pathMatch(.*)*',
      redirect: '/',
    },
  ],
})

export default router

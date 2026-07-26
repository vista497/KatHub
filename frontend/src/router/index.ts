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
    // Settings sub-routes
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
    {
      path: '/settings/cron',
      name: 'settings-cron',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/settings/skills',
      name: 'settings-skills',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/settings/models',
      name: 'settings-models',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/settings/system',
      name: 'settings-system',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/settings/agents',
      name: 'settings-agents',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/settings/kanban',
      name: 'settings-kanban',
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

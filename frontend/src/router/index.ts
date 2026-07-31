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
    // Панели (навигация из левого сайдбара / DesktopView)
    {
      path: '/crew',
      name: 'crew',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/kanban',
      name: 'kanban',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/cron',
      name: 'cron',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/content',
      name: 'content',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/galaxy',
      name: 'galaxy',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/skills',
      name: 'skills',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/models',
      name: 'models',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/system',
      name: 'system',
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/agents',
      name: 'agents',
      component: () => import('../views/DesktopView.vue'),
    },
    // Старые пути /settings/* → новые (редиректы для старых ссылок)
    { path: '/settings/cron', redirect: '/cron' },
    { path: '/settings/kanban', redirect: '/kanban' },
    { path: '/settings/skills', redirect: '/skills' },
    { path: '/settings/models', redirect: '/models' },
    { path: '/settings/system', redirect: '/system' },
    { path: '/settings/agents', redirect: '/agents' },
    { path: '/settings/plugins', redirect: '/' },
    { path: '/settings/backends', redirect: '/' },
    { path: '/settings/theme', redirect: '/' },
    // Catch-all redirect
    {
      path: '/:pathMatch(.*)*',
      redirect: '/',
    },
  ],
})

export default router

import { createRouter, createWebHistory } from 'vue-router'
import DashboardView from '../views/DashboardView.vue'
import SettingsView from '../views/SettingsView.vue'
import ProvidersView from '../views/ProvidersView.vue'

const routes = [
  {
    path: '/',
    name: 'dashboard',
    component: DashboardView,
  },
  {
    path: '/settings',
    name: 'settings',
    component: SettingsView,
  },
  {
    path: '/providers',
    name: 'providers',
    component: ProvidersView,
  },
]

const router = createRouter({
  history: createWebHistory(),
  routes,
})

export default router

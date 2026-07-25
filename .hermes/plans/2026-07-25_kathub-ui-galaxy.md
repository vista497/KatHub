# KatHub UI — Memory Galaxy + Sidebar + Chat Overlay

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Заменить заглушку HelloWorld на полноценный трёхпанельный UI с галактической визуализацией памяти Obsidian, деревом навигации слева и оверлей-чатом справа.

**Architecture:** Vue 3 SPA с CSS Grid-раскладкой. Центральный компонент — D3.js force simulation графа vault'а, стилизованный под космос/галактику. Слева — collapsible sidebar с группировкой: Vault-навигатор, AI-сессии, Настройки. Справа — Telegram-подобный чат-оверлей, скрытый за полосками-виджетами. Responsive: mobile — чат на весь экран, десктоп — три панели.

**Tech Stack:** Vue 3.5 + TypeScript 6 + Vite 8, D3.js v7, Pinia (state), Vue Router, CSS Custom Properties (темизация)

---

## Основные принципы

- **Компонентная архитектура** — каждый виджет заменим независимо
- **CSS Custom Properties** — тёмная/светлая/кастомная тема из одного источника
- **Mobile-first responsive** — базовые стили для mobile, десктоп через min-width
- **Чистые компоненты** — без хардкода, данные через props/stores
- **API-first** — graph данные через `/api/vault/graph`, чат через WebSocket

---

## Схема компонентов (desktop)

```
┌────────────────────────────────────────────────────────┐
│ App.vue (CSS Grid: sidebar | graph | chat-overlay)     │
│              ┌──────────────────┬──────────────────────┤
│ SidebarPanel │  GalaxyGraph     │  ChatOverlay         │
│ ───────────  │  (D3.js canvas)  │  ──────────────      │
│ 🏠 Home      │                  │  ▌Катя               │
│ 📁 Vault     │    ✦  ✦         │  ▌twin               │
│   Диалоги/   │  ✦    ✦  ✦      │  ▌doctor             │
│   Проекты/   │    ✦  ✦  ✦      │  ┌──────────────┐    │
│   _meta/     │  ✦    ✦         │  │ ChatPanel    │    │
│ 🤖 AI        │    ✦  ✦         │  │ msg1 msg2..  │    │
│   Катя       │                  │  │ [________]   │    │
│   twin       │                  │  └──────────────┘    │
│ ⚙️ Settings  │                  │                      │
│   Плагины    │                  │                      │
│   Бэкенды    │                  │                      │
│   Тема       │                  │                      │
└──────────────┴──────────────────┴──────────────────────┘
```

### Mobile

```
┌─────────────────────┐
│ ChatView (full)     │
│ msg                 │
│ msg                 │
│ msg                 │
│ [___________]       │
│                     │
│ [≡] [◎] [⚙]        │ ← bottom nav
└─────────────────────┘
```

---

## Задачи

### Task 1: Установка зависимостей (Pinia, Vue Router, D3.js)

**Objective:** Добавить необходимые npm-пакеты

**Files:**
- Modify: `frontend/package.json`

**Step 1: Установить пакеты**

```bash
cd /d/vs/Project421/1_projectGPT/1_Main/KatHub/frontend
npm install pinia vue-router@4 d3 @types/d3
```

**Step 2: Проверить package.json**

```bash
cat package.json | grep -E "pinia|vue-router|d3"
```

Ожидаемый результат: три пакета в dependencies.

---

### Task 2: Тема — CSS Custom Properties + переключатель

**Objective:** Создать систему тем с светлой/тёмной и заделом на кастомную.

**Files:**
- Create: `frontend/src/assets/themes/base.css`
- Create: `frontend/src/assets/themes/dark.css`
- Create: `frontend/src/assets/themes/light.css`
- Create: `frontend/src/stores/themeStore.ts`
- Modify: `frontend/src/main.ts` (подключить Pinia)

**Step 1: base.css — CSS custom properties по умолчанию**

```css
:root {
  /* Core */
  --color-bg-primary: #0a0a1a;
  --color-bg-secondary: #12122a;
  --color-bg-tertiary: #1a1a3e;
  --color-surface: rgba(255, 255, 255, 0.05);
  --color-surface-hover: rgba(255, 255, 255, 0.08);
  --color-border: rgba(255, 255, 255, 0.08);
  
  /* Text */
  --color-text-primary: #e8e8f0;
  --color-text-secondary: #8888aa;
  --color-text-muted: #555577;
  
  /* Accent */
  --color-accent: #7c5cff;
  --color-accent-glow: rgba(124, 92, 255, 0.3);
  --color-accent-secondary: #5ce0ff;
  
  /* Chat */
  --color-chat-bg: #0d0d24;
  --color-chat-bubble-user: #1e1e4a;
  --color-chat-bubble-assistant: #111133;
  --color-chat-strip: rgba(124, 92, 255, 0.15);
  --color-chat-strip-hover: rgba(124, 92, 255, 0.4);
  
  /* Graph */
  --color-node-default: #7c5cff;
  --color-node-folder: #ff6b9d;
  --color-node-tag: #5ce0ff;
  --color-link: rgba(124, 92, 255, 0.15);
  
  /* Layout */
  --sidebar-width: 280px;
  --chat-strip-width: 8px;
  --chat-panel-width: 420px;
  
  /* Transitions */
  --transition-fast: 150ms ease;
  --transition-normal: 250ms ease;
  --transition-slow: 400ms cubic-bezier(0.4, 0, 0.2, 1);
  
  /* Typography */
  --font-sans: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
  --font-mono: 'JetBrains Mono', 'Fira Code', monospace;
  --font-size-xs: 0.75rem;
  --font-size-sm: 0.875rem;
  --font-size-base: 1rem;
  --font-size-lg: 1.125rem;
  --font-size-xl: 1.5rem;
  
  /* Spacing */
  --space-1: 4px;
  --space-2: 8px;
  --space-3: 12px;
  --space-4: 16px;
  --space-6: 24px;
  --space-8: 32px;
  
  /* Radius */
  --radius-sm: 6px;
  --radius-md: 10px;
  --radius-lg: 16px;
  --radius-full: 9999px;
}
```

**Step 2: light.css — светлая тема (переопределения)**

```css
[data-theme="light"] {
  --color-bg-primary: #f5f5fa;
  --color-bg-secondary: #eaeaef;
  --color-bg-tertiary: #d5d5e0;
  --color-surface: rgba(0, 0, 0, 0.03);
  --color-surface-hover: rgba(0, 0, 0, 0.05);
  --color-border: rgba(0, 0, 0, 0.08);
  --color-text-primary: #1a1a2e;
  --color-text-secondary: #555577;
  --color-text-muted: #8888aa;
  --color-chat-bg: #eeeeff;
  --color-chat-bubble-user: #e0e0f5;
  --color-chat-bubble-assistant: #f0f0fa;
}
```

**Step 3: themeStore.ts**

```typescript
import { defineStore } from 'pinia'
import { ref, watch } from 'vue'

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

  // Применить при инициализации
  apply()

  return { current, setTheme, toggle }
})
```

**Step 4: main.ts — подключить Pinia**

```typescript
import { createApp } from 'vue'
import { createPinia } from 'pinia'
import App from './App.vue'
import './assets/themes/base.css'
import './assets/themes/dark.css'
import './assets/themes/light.css'

const app = createApp(App)
app.use(createPinia())
app.mount('#app')
```

---

### Task 3: Vue Router — структура маршрутов

**Objective:** Настроить Vue Router с desktop и mobile раскладками.

**Files:**
- Create: `frontend/src/router/index.ts`
- Modify: `frontend/src/main.ts` (use router)
- Create: `frontend/src/views/DesktopView.vue`
- Create: `frontend/src/views/MobileView.vue`

**Step 1: router/index.ts**

```typescript
import { createRouter, createWebHashHistory } from 'vue-router'

const router = createRouter({
  history: createWebHashHistory(),
  routes: [
    {
      path: '/',
      name: 'home',
      // Авто-определение: desktop или mobile
      component: () => import('../views/DesktopView.vue'),
    },
    {
      path: '/chat',
      name: 'chat',
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
```

**Step 2: DesktopView.vue — 3-column grid skeleton**

```vue
<script setup lang="ts">
import SidebarPanel from '../components/layout/SidebarPanel.vue'
import GalaxyGraph from '../components/graph/GalaxyGraph.vue'
import ChatOverlay from '../components/chat/ChatOverlay.vue'
</script>

<template>
  <div class="desktop-layout">
    <SidebarPanel class="sidebar" />
    <GalaxyGraph class="graph" />
    <ChatOverlay class="chat-overlay" />
  </div>
</template>

<style scoped>
.desktop-layout {
  display: grid;
  grid-template-columns: var(--sidebar-width) 1fr auto;
  grid-template-rows: 100vh;
  overflow: hidden;
  background: var(--color-bg-primary);
  color: var(--color-text-primary);
  font-family: var(--font-sans);
}

@media (max-width: 1023px) {
  .desktop-layout {
    display: none; /* mobile view takes over */
  }
}
</style>
```

**Step 3: MobileView.vue — skeleton с bottom nav**

```vue
<script setup lang="ts">
import { ref } from 'vue'
import ChatView from '../components/chat/ChatView.vue'
import GalaxyGraph from '../components/graph/GalaxyGraph.vue'
import SidebarPanel from '../components/layout/SidebarPanel.vue'

const activeTab = ref<'chat' | 'graph' | 'sidebar'>('chat')
</script>

<template>
  <div class="mobile-layout">
    <div class="mobile-content">
      <ChatView v-if="activeTab === 'chat'" />
      <GalaxyGraph v-else-if="activeTab === 'graph'" />
      <SidebarPanel v-else-if="activeTab === 'sidebar'" />
    </div>
    <nav class="bottom-nav">
      <button :class="{ active: activeTab === 'chat' }" @click="activeTab = 'chat'">
        💬
      </button>
      <button :class="{ active: activeTab === 'graph' }" @click="activeTab = 'graph'">
        ◎
      </button>
      <button :class="{ active: activeTab === 'sidebar' }" @click="activeTab = 'sidebar'">
        ≡
      </button>
    </nav>
  </div>
</template>

<style scoped>
@media (min-width: 1024px) {
  .mobile-layout { display: none; }
}
.mobile-layout {
  height: 100dvh;
  display: flex;
  flex-direction: column;
  background: var(--color-bg-primary);
  color: var(--color-text-primary);
}
.mobile-content { flex: 1; overflow: hidden; }
.bottom-nav {
  display: flex;
  justify-content: space-around;
  padding: var(--space-2);
  background: var(--color-bg-secondary);
  border-top: 1px solid var(--color-border);
}
.bottom-nav button {
  background: none;
  border: none;
  font-size: 1.5rem;
  padding: var(--space-2) var(--space-4);
  border-radius: var(--radius-md);
  cursor: pointer;
  transition: background var(--transition-fast);
}
.bottom-nav button.active {
  background: var(--color-accent-glow);
}
</style>
```

**Step 4: main.ts — добавить router**

```typescript
import { createApp } from 'vue'
import { createPinia } from 'pinia'
import router from './router'
import App from './App.vue'
import './assets/themes/base.css'
import './assets/themes/dark.css'
import './assets/themes/light.css'

const app = createApp(App)
app.use(createPinia())
app.use(router)
app.mount('#app')
```

**Step 5: App.vue**

```vue
<template>
  <router-view />
</template>
```

---

### Task 4: SidebarPanel — левая панель с деревом

**Objective:** Боковая панель с секциями: Vault-навигатор, AI-сессии, Настройки.

**Files:**
- Create: `frontend/src/components/layout/SidebarPanel.vue`
- Create: `frontend/src/components/layout/SidebarSection.vue`
- Create: `frontend/src/components/layout/VaultTree.vue`
- Create: `frontend/src/components/layout/AiSessions.vue`
- Create: `frontend/src/stores/sidebarStore.ts`

**Step 1: sidebarStore.ts**

```typescript
import { defineStore } from 'pinia'
import { ref } from 'vue'

export const useSidebarStore = defineStore('sidebar', () => {
  const collapsed = ref(false)
  const expandedSections = ref<Set<string>>(new Set(['vault']))

  function toggle() {
    collapsed.value = !collapsed.value
  }

  function toggleSection(name: string) {
    if (expandedSections.value.has(name)) {
      expandedSections.value.delete(name)
    } else {
      expandedSections.value.add(name)
    }
  }

  return { collapsed, expandedSections, toggle, toggleSection }
})
```

**Step 2: SidebarPanel.vue — основной контейнер**

```vue
<script setup lang="ts">
import { useSidebarStore } from '../../stores/sidebarStore'
import SidebarSection from './SidebarSection.vue'
import VaultTree from './VaultTree.vue'
import AiSessions from './AiSessions.vue'

const sidebar = useSidebarStore()
</script>

<template>
  <aside class="sidebar" :class="{ collapsed: sidebar.collapsed }">
    <div class="sidebar-header">
      <span class="logo" v-if="!sidebar.collapsed">✦ KatHub</span>
      <button class="collapse-btn" @click="sidebar.toggle">
        {{ sidebar.collapsed ? '▶' : '◀' }}
      </button>
    </div>
    
    <div class="sidebar-content" v-if="!sidebar.collapsed">
      <SidebarSection title="Vault" name="vault">
        <VaultTree />
      </SidebarSection>
      
      <SidebarSection title="AI" name="ai">
        <AiSessions />
      </SidebarSection>
      
      <SidebarSection title="Settings" name="settings">
        <div class="settings-list">
          <router-link to="/settings/plugins">Plugins</router-link>
          <router-link to="/settings/backends">Backends</router-link>
          <router-link to="/settings/theme">Theme</router-link>
        </div>
      </SidebarSection>
    </div>
  </aside>
</template>

<style scoped>
.sidebar {
  width: var(--sidebar-width);
  height: 100vh;
  background: var(--color-bg-secondary);
  border-right: 1px solid var(--color-border);
  display: flex;
  flex-direction: column;
  transition: width var(--transition-normal);
  overflow: hidden;
}
.sidebar.collapsed { width: 40px; }
.sidebar-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: var(--space-4);
  border-bottom: 1px solid var(--color-border);
}
.logo { font-size: var(--font-size-lg); font-weight: 700; color: var(--color-accent); }
.collapse-btn {
  background: none;
  border: none;
  color: var(--color-text-secondary);
  cursor: pointer;
  font-size: var(--font-size-sm);
}
.sidebar-content {
  flex: 1;
  overflow-y: auto;
  padding: var(--space-2);
}
.settings-list { display: flex; flex-direction: column; gap: var(--space-1); }
.settings-list a {
  color: var(--color-text-secondary);
  text-decoration: none;
  padding: var(--space-2) var(--space-3);
  border-radius: var(--radius-sm);
  font-size: var(--font-size-sm);
  transition: all var(--transition-fast);
}
.settings-list a:hover {
  background: var(--color-surface-hover);
  color: var(--color-text-primary);
}
</style>
```

**Step 3: SidebarSection.vue**

```vue
<script setup lang="ts">
import { useSidebarStore } from '../../stores/sidebarStore'

const props = defineProps<{ title: string; name: string }>()
const sidebar = useSidebarStore()
</script>

<template>
  <div class="section">
    <button class="section-header" @click="sidebar.toggleSection(name)">
      <span class="arrow">{{ sidebar.expandedSections.has(name) ? '▾' : '▸' }}</span>
      <span class="title">{{ title }}</span>
    </button>
    <div class="section-body" v-show="sidebar.expandedSections.has(name)">
      <slot />
    </div>
  </div>
</template>

<style scoped>
.section { margin-bottom: var(--space-1); }
.section-header {
  display: flex;
  align-items: center;
  gap: var(--space-2);
  width: 100%;
  padding: var(--space-2) var(--space-3);
  background: none;
  border: none;
  color: var(--color-text-secondary);
  cursor: pointer;
  font-size: var(--font-size-xs);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}
.section-header:hover { color: var(--color-text-primary); }
.arrow { font-size: 10px; width: 12px; }
.section-body { padding: var(--space-1) 0 var(--space-2) var(--space-6); }
</style>
```

**Step 4: VaultTree.vue — заглушка (позже заменим на API)**

```vue
<script setup lang="ts">
const folders = [
  { name: 'Диалоги', count: 12 },
  { name: 'Проекты', count: 3 },
  { name: '_meta', count: 5 },
  { name: 'Агенты', count: 4 },
]
</script>

<template>
  <div class="vault-tree">
    <div v-for="folder in folders" :key="folder.name" class="folder-item">
      📁 {{ folder.name }}
      <span class="count">{{ folder.count }}</span>
    </div>
  </div>
</template>

<style scoped>
.folder-item {
  display: flex;
  align-items: center;
  gap: var(--space-2);
  padding: var(--space-1) var(--space-2);
  font-size: var(--font-size-sm);
  color: var(--color-text-secondary);
  border-radius: var(--radius-sm);
  cursor: pointer;
  transition: all var(--transition-fast);
}
.folder-item:hover { background: var(--color-surface-hover); color: var(--color-text-primary); }
.count { margin-left: auto; font-size: var(--font-size-xs); color: var(--color-text-muted); }
</style>
```

**Step 5: AiSessions.vue — заглушка**

```vue
<script setup lang="ts">
const agents = ['Катя', 'twin', 'reflection', 'doctor']
</script>

<template>
  <div class="ai-sessions">
    <div v-for="agent in agents" :key="agent" class="agent-item">
      <span class="dot"></span>
      {{ agent }}
    </div>
  </div>
</template>

<style scoped>
.agent-item {
  display: flex;
  align-items: center;
  gap: var(--space-2);
  padding: var(--space-2) var(--space-3);
  font-size: var(--font-size-sm);
  color: var(--color-text-secondary);
  border-radius: var(--radius-sm);
  cursor: pointer;
  transition: all var(--transition-fast);
}
.agent-item:hover { background: var(--color-surface-hover); color: var(--color-text-primary); }
.dot {
  width: 6px; height: 6px;
  border-radius: 50%;
  background: var(--color-accent-secondary);
}
</style>
```

---

### Task 5: GalaxyGraph — D3.js визуализация vault'а

**Objective:** Интерактивный граф заметок Obsidian, стилизованный под галактику.

**Files:**
- Create: `frontend/src/components/graph/GalaxyGraph.vue`
- Create: `frontend/src/stores/graphStore.ts`

**Step 1: graphStore.ts**

```typescript
import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

interface GraphNode {
  id: string
  label: string
  type: 'note' | 'folder' | 'tag'
  folder?: string
  x?: number
  y?: number
  fx?: number | null
  fy?: number | null
}

interface GraphLink {
  source: string
  target: string
  type: 'wikilink' | 'folder' | 'tag'
}

export const useGraphStore = defineStore('graph', () => {
  const nodes = ref<GraphNode[]>([])
  const links = ref<GraphLink[]>([])
  const loading = ref(false)
  const selectedNode = ref<string | null>(null)

  async function fetchGraph() {
    loading.value = true
    try {
      const res = await fetch('/api/vault/graph')
      const data = await res.json()
      nodes.value = data.nodes
      links.value = data.links
    } catch (e) {
      console.error('Failed to load graph:', e)
    } finally {
      loading.value = false
    }
  }

  function selectNode(id: string | null) {
    selectedNode.value = id
  }

  return { nodes, links, loading, selectedNode, fetchGraph, selectNode }
})
```

**Step 2: GalaxyGraph.vue — D3.js force simulation**

```vue
<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch } from 'vue'
import * as d3 from 'd3'
import { useGraphStore } from '../../stores/graphStore'

const container = ref<HTMLDivElement>()
const svg = ref<SVGSVGElement>()
const graph = useGraphStore()

// Force simulation + D3 rendering
let simulation: d3.Simulation<any, any> | null = null

function initGraph() {
  if (!svg.value || !graph.nodes.length) return
  
  const width = container.value!.clientWidth
  const height = container.value!.clientHeight
  
  // Clear previous
  d3.select(svg.value).selectAll('*').remove()
  
  const svgEl = d3.select(svg.value)
    .attr('width', width)
    .attr('height', height)
  
  // Zoom
  const g = svgEl.append('g')
  svgEl.call(d3.zoom<any, unknown>()
    .scaleExtent([0.1, 5])
    .on('zoom', (event) => {
      g.attr('transform', event.transform)
    }))
  
  // Links
  const link = g.append('g')
    .selectAll('line')
    .data(graph.links)
    .join('line')
    .attr('stroke', 'var(--color-link)')
    .attr('stroke-width', 0.5)
    .attr('stroke-opacity', 0.4)
  
  // Nodes
  const node = g.append('g')
    .selectAll('g')
    .data(graph.nodes)
    .join('g')
    .attr('cursor', 'pointer')
    .call(d3.drag<any, any>()
      .on('start', (event, d) => {
        if (!event.active) simulation!.alphaTarget(0.3).restart()
        d.fx = d.x
        d.fy = d.y
      })
      .on('drag', (event, d) => {
        d.fx = event.x
        d.fy = event.y
      })
      .on('end', (event, d) => {
        if (!event.active) simulation!.alphaTarget(0)
        d.fx = null
        d.fy = null
      }))
  
  // Node circles — styled like stars
  node.append('circle')
    .attr('r', (d: any) => d.type === 'folder' ? 6 : d.type === 'tag' ? 4 : 3)
    .attr('fill', (d: any) => {
      switch (d.type) {
        case 'folder': return 'var(--color-node-folder)'
        case 'tag': return 'var(--color-node-tag)'
        default: return 'var(--color-node-default)'
      }
    })
    .attr('opacity', 0.8)
  
  // Glow effect
  node.append('circle')
    .attr('r', (d: any) => d.type === 'folder' ? 12 : 8)
    .attr('fill', (d: any) => {
      switch (d.type) {
        case 'folder': return 'var(--color-node-folder)'
        case 'tag': return 'var(--color-node-tag)'
        default: return 'var(--color-node-default)'
      }
    })
    .attr('opacity', 0.08)
  
  // Labels (only for folders and selected)
  node.append('text')
    .text((d: any) => d.label)
    .attr('font-size', '8px')
    .attr('fill', 'var(--color-text-secondary)')
    .attr('dx', 8)
    .attr('dy', 3)
    .attr('opacity', (d: any) => d.type === 'folder' ? 0.7 : 0)
  
  // Click handler
  node.on('click', (_event: any, d: any) => {
    graph.selectNode(d.id)
  })
  
  // Force simulation
  simulation = d3.forceSimulation(graph.nodes)
    .force('link', d3.forceLink(graph.links).id((d: any) => d.id).distance(60))
    .force('charge', d3.forceManyBody().strength(-120))
    .force('center', d3.forceCenter(width / 2, height / 2))
    .force('collision', d3.forceCollide().radius(12))
    .on('tick', () => {
      link
        .attr('x1', (d: any) => d.source.x)
        .attr('y1', (d: any) => d.source.y)
        .attr('x2', (d: any) => d.target.x)
        .attr('y2', (d: any) => d.target.y)
      node.attr('transform', (d: any) => `translate(${d.x},${d.y})`)
    })
}

// Resize
function onResize() { initGraph() }

onMounted(() => {
  window.addEventListener('resize', onResize)
  graph.fetchGraph()
})

onUnmounted(() => {
  window.removeEventListener('resize', onResize)
  simulation?.stop()
})

watch(() => graph.nodes.length, () => {
  if (graph.nodes.length) initGraph()
})
</script>

<template>
  <div ref="container" class="graph-container">
    <!-- Background stars (static, generated via CSS) -->
    <div class="starfield"></div>
    
    <!-- Loading -->
    <div v-if="graph.loading" class="loading">Loading galaxy...</div>
    
    <!-- D3 SVG -->
    <svg ref="svg" class="graph-svg"></svg>
    
    <!-- Selected node info -->
    <div v-if="graph.selectedNode" class="node-info">
      {{ graph.selectedNode }}
    </div>
  </div>
</template>

<style scoped>
.graph-container {
  position: relative;
  width: 100%;
  height: 100%;
  overflow: hidden;
}

.starfield {
  position: absolute;
  inset: 0;
  background: 
    radial-gradient(1px 1px at 10% 20%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 30% 60%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 50% 10%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1px 1px at 70% 40%, rgba(255,255,255,0.2), transparent),
    radial-gradient(1px 1px at 90% 80%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1.5px 1.5px at 20% 80%, rgba(124,92,255,0.4), transparent),
    radial-gradient(1.5px 1.5px at 60% 30%, rgba(92,224,255,0.3), transparent),
    radial-gradient(1px 1px at 80% 55%, rgba(255,255,255,0.5), transparent);
  /* Можно сгенерировать больше через JS или псевдоэлементы */
  pointer-events: none;
}

.graph-svg {
  position: absolute;
  inset: 0;
}

.loading {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  color: var(--color-text-secondary);
  font-size: var(--font-size-lg);
}

.node-info {
  position: absolute;
  bottom: var(--space-4);
  left: 50%;
  transform: translateX(-50%);
  padding: var(--space-2) var(--space-4);
  background: var(--color-bg-tertiary);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-full);
  font-size: var(--font-size-sm);
  color: var(--color-text-secondary);
}
</style>
```

---

### Task 6: ChatOverlay — правая панель чата

**Objective:** Чат-панель с полосками-виджетами справа, открывается по клику.

**Files:**
- Create: `frontend/src/components/chat/ChatOverlay.vue`
- Create: `frontend/src/components/chat/ChatPanel.vue`
- Create: `frontend/src/components/chat/ChatMessage.vue`
- Create: `frontend/src/components/chat/ChatInput.vue`
- Create: `frontend/src/stores/chatStore.ts`

**Step 1: chatStore.ts**

```typescript
import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

interface Message {
  id: string
  role: 'user' | 'assistant'
  content: string
  timestamp: Date
}

interface ChatSession {
  id: string
  agent: string
  messages: Message[]
  unread: number
}

export const useChatStore = defineStore('chat', () => {
  const sessions = ref<ChatSession[]>([
    { id: 'katya', agent: 'Катя', messages: [], unread: 0 },
    { id: 'twin', agent: 'twin', messages: [], unread: 0 },
    { id: 'doctor', agent: 'doctor', messages: [], unread: 0 },
  ])
  const activeSession = ref<string | null>(null)
  const panelOpen = ref(false)

  function openSession(id: string) {
    activeSession.value = id
    panelOpen.value = true
    const s = sessions.value.find(s => s.id === id)
    if (s) s.unread = 0
  }

  function closePanel() {
    panelOpen.value = false
  }

  function sendMessage(content: string) {
    if (!activeSession.value) return
    const session = sessions.value.find(s => s.id === activeSession.value)
    if (!session) return
    session.messages.push({
      id: crypto.randomUUID(),
      role: 'user',
      content,
      timestamp: new Date(),
    })
    // Ответ ассистента — позже через WebSocket
  }

  return { sessions, activeSession, panelOpen, openSession, closePanel, sendMessage }
})
```

**Step 2: ChatOverlay.vue — полоски + панель**

```vue
<script setup lang="ts">
import { useChatStore } from '../../stores/chatStore'
import ChatPanel from './ChatPanel.vue'

const chat = useChatStore()
</script>

<template>
  <div class="chat-overlay">
    <!-- Полоски-виджеты -->
    <div class="chat-strips">
      <div
        v-for="session in chat.sessions"
        :key="session.id"
        class="chat-strip"
        :class="{ active: chat.activeSession === session.id }"
        @click="chat.openSession(session.id)"
      >
        <span class="strip-label">{{ session.agent[0] }}</span>
        <span v-if="session.unread" class="unread-badge">{{ session.unread }}</span>
      </div>
    </div>

    <!-- Панель чата -->
    <Transition name="slide">
      <ChatPanel v-if="chat.panelOpen" @close="chat.closePanel" />
    </Transition>
  </div>
</template>

<style scoped>
.chat-overlay {
  display: flex;
  height: 100vh;
}

.chat-strips {
  display: flex;
  flex-direction: column;
  gap: var(--space-1);
  padding: var(--space-2);
  background: var(--color-bg-secondary);
  border-left: 1px solid var(--color-border);
}

.chat-strip {
  width: 28px;
  height: 60px;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  border-radius: var(--radius-sm);
  background: var(--color-chat-strip);
  cursor: pointer;
  transition: all var(--transition-fast);
  position: relative;
}
.chat-strip:hover {
  background: var(--color-chat-strip-hover);
  width: 34px;
}
.chat-strip.active {
  background: var(--color-accent);
}

.strip-label {
  font-size: var(--font-size-sm);
  color: var(--color-text-primary);
  font-weight: 600;
}

.unread-badge {
  position: absolute;
  top: 4px;
  right: 4px;
  width: 8px;
  height: 8px;
  background: var(--color-accent-secondary);
  border-radius: 50%;
  font-size: 9px;
  display: flex;
  align-items: center;
  justify-content: center;
}

/* Panel slide animation */
.slide-enter-active, .slide-leave-active {
  transition: all var(--transition-slow);
}
.slide-enter-from, .slide-leave-to {
  transform: translateX(100%);
  opacity: 0;
}
</style>
```

**Step 3: ChatPanel.vue — интерфейс Telegram-стиля**

```vue
<script setup lang="ts">
import { ref } from 'vue'
import { useChatStore } from '../../stores/chatStore'
import ChatMessage from './ChatMessage.vue'
import ChatInput from './ChatInput.vue'

const chat = useChatStore()
defineEmits<{ close: [] }>()

const session = computed(() =>
  chat.sessions.find(s => s.id === chat.activeSession)
)
</script>

<template>
  <div class="chat-panel">
    <!-- Header -->
    <div class="chat-header">
      <span class="agent-name">{{ session?.agent || 'Chat' }}</span>
      <button class="close-btn" @click="$emit('close')">✕</button>
    </div>

    <!-- Messages -->
    <div class="chat-messages" ref="messagesContainer">
      <ChatMessage
        v-for="msg in session?.messages"
        :key="msg.id"
        :message="msg"
      />
    </div>

    <!-- Input -->
    <ChatInput @send="chat.sendMessage" />
  </div>
</template>

<style scoped>
.chat-panel {
  width: var(--chat-panel-width);
  height: 100vh;
  display: flex;
  flex-direction: column;
  background: var(--color-chat-bg);
  border-left: 1px solid var(--color-border);
}

.chat-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: var(--space-3) var(--space-4);
  border-bottom: 1px solid var(--color-border);
  background: var(--color-bg-secondary);
}

.agent-name {
  font-weight: 600;
  font-size: var(--font-size-base);
}

.close-btn {
  background: none;
  border: none;
  color: var(--color-text-secondary);
  cursor: pointer;
  font-size: var(--font-size-lg);
}
.close-btn:hover { color: var(--color-text-primary); }

.chat-messages {
  flex: 1;
  overflow-y: auto;
  padding: var(--space-4);
  display: flex;
  flex-direction: column;
  gap: var(--space-2);
}
</style>
```

**Step 4: ChatMessage.vue**

```vue
<script setup lang="ts">
defineProps<{
  message: { id: string; role: string; content: string; timestamp: Date }
}>()
</script>

<template>
  <div class="message" :class="message.role">
    <div class="bubble">
      {{ message.content }}
    </div>
    <div class="time">
      {{ new Date(message.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) }}
    </div>
  </div>
</template>

<style scoped>
.message {
  display: flex;
  flex-direction: column;
  max-width: 85%;
}
.message.user { align-self: flex-end; }
.message.assistant { align-self: flex-start; }

.bubble {
  padding: var(--space-2) var(--space-3);
  border-radius: var(--radius-md);
  font-size: var(--font-size-sm);
  line-height: 1.5;
}
.user .bubble { background: var(--color-accent); color: white; border-bottom-right-radius: var(--space-1); }
.assistant .bubble { background: var(--color-chat-bubble-assistant); border-bottom-left-radius: var(--space-1); }

.time {
  font-size: var(--font-size-xs);
  color: var(--color-text-muted);
  margin-top: 2px;
  padding: 0 var(--space-1);
}
</style>
```

**Step 5: ChatInput.vue**

```vue
<script setup lang="ts">
import { ref } from 'vue'

const emit = defineEmits<{ send: [text: string] }>()
const text = ref('')

function handleSend() {
  if (!text.value.trim()) return
  emit('send', text.value)
  text.value = ''
}
</script>

<template>
  <div class="chat-input">
    <textarea
      v-model="text"
      placeholder="Message..."
      rows="1"
      @keydown.enter.exact.prevent="handleSend"
      @input="(e) => {
        const target = e.target as HTMLTextAreaElement
        target.style.height = 'auto'
        target.style.height = target.scrollHeight + 'px'
      }"
    ></textarea>
    <button class="send-btn" @click="handleSend">↑</button>
  </div>
</template>

<style scoped>
.chat-input {
  display: flex;
  align-items: flex-end;
  gap: var(--space-2);
  padding: var(--space-3) var(--space-4);
  border-top: 1px solid var(--color-border);
  background: var(--color-bg-secondary);
}
textarea {
  flex: 1;
  resize: none;
  max-height: 120px;
  padding: var(--space-2) var(--space-3);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-lg);
  background: var(--color-chat-bg);
  color: var(--color-text-primary);
  font-family: var(--font-sans);
  font-size: var(--font-size-sm);
  outline: none;
}
textarea:focus { border-color: var(--color-accent); }
.send-btn {
  width: 36px; height: 36px;
  border-radius: 50%;
  background: var(--color-accent);
  color: white;
  border: none;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: var(--font-size-lg);
  transition: transform var(--transition-fast);
}
.send-btn:hover { transform: scale(1.1); }
</style>
```

---

### Task 7: Backend `/api/vault/graph` endpoint

**Objective:** Добавить HTTP-endpoint, который читает Obsidian vault и возвращает JSON с nodes + links для D3.js.

**Files:**
- Create: `backend/handlers/VaultGraphHandler.h`
- Create: `backend/handlers/VaultGraphHandler.cpp`
- Modify: `backend/handlers/CMakeLists.txt`
- Modify: `backend/KatHubApp.cpp` (register handler)

**Step 1: VaultGraphHandler.h**

```cpp
#pragma once
#include "domain/IHttpHandler.h"
#include <string>

namespace KatHub {

class VaultGraphHandler : public IHttpHandler {
public:
  explicit VaultGraphHandler(const std::string& vaultPath);
  
  const char* path() const override { return "/api/vault/graph"; }
  const char* method() const override { return "GET"; }
  void handle(const HttpRequest& req, HttpResponse& res) override;

private:
  std::string vaultPath_;
  std::string readFile(const std::string& path);
  std::string extractWikilinks(const std::string& content);
  bool isMarkdown(const std::string& path);
};

} // namespace KatHub
```

**Step 2: VaultGraphHandler.cpp — парсинг vault'а**

```cpp
#include "VaultGraphHandler.h"
#include <filesystem>
#include <regex>
#include <sstream>
#include <fstream>

namespace fs = std::filesystem;

namespace KatHub {

VaultGraphHandler::VaultGraphHandler(const std::string& vaultPath)
  : vaultPath_(vaultPath) {}

void VaultGraphHandler::handle(const HttpRequest& req, HttpResponse& res) {
  nlohmann::json body;
  body["nodes"] = nlohmann::json::array();
  body["links"] = nlohmann::json::array();
  
  if (!fs::exists(vaultPath_)) {
    res.set_content(body.dump(), "application/json");
    return;
  }
  
  // 1. Папки как ноды
  std::map<std::string, int> folderColors;
  int colorIdx = 0;
  const char* folderColorsHex[] = {
    "#ff6b9d", "#5ce0ff", "#ffd93d", "#6bff6b",
    "#ff6b6b", "#c46bff", "#6bffd9", "#ff9f43"
  };
  
  for (const auto& entry : fs::directory_iterator(vaultPath_)) {
    if (entry.is_directory() && entry.path().filename().string()[0] != '.') {
      std::string folder = entry.path().filename().string();
      body["nodes"].push_back({
        {"id", folder},
        {"label", folder},
        {"type", "folder"},
        {"folder", folder}
      });
      folderColors[folder] = colorIdx++ % 8;
    }
  }
  
  // 2. .md файлы как ноды + поиск wikilinks [[...]]
  std::regex wikilink_re(R"(\[\[([^\]]+)\]\])");
  
  for (const auto& entry : fs::recursive_directory_iterator(vaultPath_)) {
    if (!entry.is_regular_file() || !isMarkdown(entry.path().string())) continue;
    
    std::string relPath = fs::relative(entry.path(), vaultPath_).string();
    std::string folder = fs::path(relPath).parent_path().string();
    if (folder == ".") folder = "root";
    
    std::string content = readFile(entry.path().string());
    std::string title = fs::path(relPath).stem().string();
    
    body["nodes"].push_back({
      {"id", relPath},
      {"label", title},
      {"type", "note"},
      {"folder", folder}
    });
    
    // Поиск wikilinks
    auto begin = std::sregex_iterator(content.begin(), content.end(), wikilink_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
      std::string targetRaw = (*it)[1].str();
      // Убираем алиас: [[page|alias]] → page
      size_t pipe = targetRaw.find('|');
      std::string target = (pipe != std::string::npos) ? targetRaw.substr(0, pipe) : targetRaw;
      
      // Пробуем найти файл
      std::string targetPath;
      if (fs::exists(vaultPath_ + "/" + target + ".md")) {
        targetPath = target + ".md";
      } else {
        // Поиск в подпапках
        for (const auto& e : fs::recursive_directory_iterator(vaultPath_)) {
          if (e.path().stem().string() == target && isMarkdown(e.path().string())) {
            targetPath = fs::relative(e.path(), vaultPath_).string();
            break;
          }
        }
      }
      
      if (!targetPath.empty()) {
        body["links"].push_back({
          {"source", relPath},
          {"target", targetPath},
          {"type", "wikilink"}
        });
      }
    }
  }
  
  res.set_content(body.dump(), "application/json");
}

std::string VaultGraphHandler::readFile(const std::string& path) {
  std::ifstream f(path);
  if (!f) return "";
  std::stringstream buf;
  buf << f.rdbuf();
  return buf.str();
}

bool VaultGraphHandler::isMarkdown(const std::string& path) {
  return path.size() >= 3 && path.substr(path.size() - 3) == ".md";
}

} // namespace KatHub
```

**Step 3: CMakeLists.txt — добавление в handlers**

```cmake
# Добавить после существующих строк в backend/handlers/CMakeLists.txt:
target_sources(kathub-handlers PRIVATE
  VaultGraphHandler.h
  VaultGraphHandler.cpp
)
```

**Step 4: KatHubApp.cpp — регистрация в Server mode**

```cpp
// В KatHubApp::initServer():
auto vaultGraphHandler = std::make_unique<VaultGraphHandler>(
  "C:/Users/User/n8n_memory/Memory/Katty_ai"  // путь к vault
);
server_->registerHandler(std::move(vaultGraphHandler));
```

**Step 5: Полная сборка и проверка**

```bash
cd /d/vs/Project421/1_projectGPT/1_Main/KatHub
cmake --build build --config Debug
# Ждать пока не запустят
```

**Step 6: Проверка endpoint**

```bash
curl http://localhost:8080/api/vault/graph | head -c 500
# Ожидаем JSON с nodes и links
```

---

### Task 8: Сборка фронтенда и интеграция

**Objective:** Собрать production-билд фронтенда, скопировать в `backend/static/`, проверить в браузере.

**Files:**
- Modify: `frontend/vite.config.ts` (base path)

**Step 1: vite.config.ts — base path для продакшена**

```typescript
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
  plugins: [vue()],
  base: './', // относительные пути — для работы из любого подкаталога
  build: {
    outDir: 'dist',
    emptyOutDir: true,
  },
})
```

**Step 2: Сборка**

```bash
cd /d/vs/Project421/1_projectGPT/1_Main/KatHub/frontend
npm run build
```

**Step 3: Копировать в backend/static/**

```bash
rm -rf /d/vs/Project421/1_projectGPT/1_Main/KatHub/backend/static/*
cp -r /d/vs/Project421/1_projectGPT/1_Main/KatHub/frontend/dist/* /d/vs/Project421/1_projectGPT/1_Main/KatHub/backend/static/
```

**Step 4: Проверка**

```bash
# Запустить сервер
cd /d/vs/Project421/1_projectGPT/1_Main/KatHub
cmake --build build --config Debug
```

Открыть http://localhost:8080 — должен показаться новый UI.

---

## Итоговая структура файлов

```
frontend/src/
├── App.vue                        # router-view
├── main.ts                        # init Pinia + Router
├── assets/themes/
│   ├── base.css                   # CSS custom properties (темная по умолчанию)
│   ├── dark.css                   # data-theme="dark" (переопределения)
│   └── light.css                  # data-theme="light" (переопределения)
├── router/
│   └── index.ts                   # Vue Router (DesktopView / MobileView)
├── stores/
│   ├── themeStore.ts              # Тема (dark/light/custom)
│   ├── sidebarStore.ts            # Состояние боковой панели
│   ├── graphStore.ts              # Данные графа vault'а
│   └── chatStore.ts               # Чат-сессии и сообщения
├── components/
│   ├── layout/
│   │   ├── SidebarPanel.vue       # Левая панель
│   │   ├── SidebarSection.vue     # Секция (collapsible)
│   │   ├── VaultTree.vue          # Дерево vault'а
│   │   └── AiSessions.vue         # Список AI-агентов
│   ├── graph/
│   │   └── GalaxyGraph.vue        # D3.js граф-галактика
│   └── chat/
│       ├── ChatOverlay.vue        # Правая панель с полосками
│       ├── ChatPanel.vue          # Панель чата (открывается)
│       ├── ChatMessage.vue        # Сообщение (пузырёк)
│       └── ChatInput.vue          # Поле ввода
└── views/
    ├── DesktopView.vue            # 3-column grid
    └── MobileView.vue             # Bottom-nav tabs

backend/handlers/
├── VaultGraphHandler.h            # HTTP handler для /api/vault/graph
├── VaultGraphHandler.cpp          # Парсинг vault → JSON nodes+links
└── CMakeLists.txt                 # + VaultGraphHandler source files
```

---

## Верификация

После выполнения всех задач:

1. `cmake --build build --config Debug` — 0 ошибок
2. `curl http://localhost:8080/api/vault/graph` — JSON с nodes и links
3. Открыть http://localhost:8080 — трёхпанельный UI
4. Клик по полоске справа — открывается чат
5. Свернуть/развернуть sidebar — кнопка ◀/▶
6. Мобильная вёрстка: открыть в Chrome DevTools (device toolbar) — bottom nav
7. Переключение темы: themeStore.setTheme('light') — светлая тема

---

## Риски и открытые вопросы

1. **Производительность D3.js** — при 1000+ нодах может тормозить. Решение: WebWorker для симуляции, Canvas вместо SVG.
2. **Vault-парсинг при старте** — может занять секунды. Решение: кешировать граф в памяти, обновлять инкрементально по watchdog.
3. **WebSocket для чата** — пока заглушка, реальная интеграция позже.
4. **Путь к vault'у** — сейчас хардкод. Нужно вынести в ConfigService / .env.

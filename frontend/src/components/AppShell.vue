<template>
  <div class="app-shell">
    <header class="top-nav">
      <div class="nav-brand">KatHub</div>
      <nav class="nav-links">
        <router-link to="/">Dashboard</router-link>
        <router-link to="/settings">Settings</router-link>
        <router-link to="/providers">Providers</router-link>
      </nav>
    </header>

    <div class="app-body">
      <aside class="sidebar">
        <ul class="sidebar-menu">
          <li><router-link to="/">Dashboard</router-link></li>
          <li><router-link to="/settings">Settings</router-link></li>
          <li><router-link to="/providers">Providers</router-link></li>
        </ul>
        <div class="sidebar-footer">
          <p>KatHub v0.1.0</p>
        </div>
      </aside>

      <main class="main-content">
        <router-view />
      </main>
    </div>
  </div>
</template>

<script setup>
// AppShell — top nav + sidebar + router-view layout
</script>

<style>
/* Reset & base */
*,
*::before,
*::after {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

html, body {
  height: 100%;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen,
    Ubuntu, Cantarell, sans-serif;
  background: #1a1a2e;
  color: #e0e0e0;
}

#app {
  height: 100%;
}
</style>

<style scoped>
.app-shell {
  display: flex;
  flex-direction: column;
  height: 100vh;
}

/* Top navigation bar */
.top-nav {
  display: flex;
  align-items: center;
  justify-content: space-between;
  height: 48px;
  padding: 0 20px;
  background: #16213e;
  border-bottom: 1px solid #0f3460;
  flex-shrink: 0;
}

.nav-brand {
  font-weight: 700;
  font-size: 1.1rem;
  color: #e94560;
}

.nav-links {
  display: flex;
  gap: 16px;
}

.nav-links a {
  color: #a0a0b0;
  text-decoration: none;
  font-size: 0.9rem;
  padding: 4px 8px;
  border-radius: 4px;
  transition: color 0.2s, background 0.2s;
}

.nav-links a:hover,
.nav-links a.router-link-active {
  color: #e94560;
  background: rgba(233, 69, 96, 0.1);
}

/* Body: sidebar + main */
.app-body {
  display: flex;
  flex: 1;
  overflow: hidden;
}

/* Sidebar */
.sidebar {
  display: flex;
  flex-direction: column;
  width: 200px;
  background: #16213e;
  border-right: 1px solid #0f3460;
  flex-shrink: 0;
}

.sidebar-menu {
  list-style: none;
  padding: 12px 0;
  flex: 1;
}

.sidebar-menu li {
  padding: 0;
}

.sidebar-menu a {
  display: block;
  padding: 8px 20px;
  color: #a0a0b0;
  text-decoration: none;
  font-size: 0.9rem;
  transition: color 0.2s, background 0.2s;
}

.sidebar-menu a:hover,
.sidebar-menu a.router-link-active {
  color: #e94560;
  background: rgba(233, 69, 96, 0.08);
}

.sidebar-footer {
  padding: 12px 20px;
  border-top: 1px solid #0f3460;
  font-size: 0.75rem;
  color: #606070;
}

/* Main content area */
.main-content {
  flex: 1;
  padding: 24px;
  overflow-y: auto;
  background: #1a1a2e;
}
</style>

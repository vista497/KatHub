import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// https://vite.dev/config/
export default defineConfig({
  plugins: [vue()],
  // Собираем СРАЗУ в backend/static — бэкенд раздаёт эту директорию.
  // Раньше был frontend/dist + симлинк/копия в backend/static — постоянная возня.
  build: {
    outDir: '../backend/static',
    // outDir вне корня проекта — vite по умолчанию НЕ чистит её.
    // Без emptyOutDir каждая сборка копит старые хеши (5 версий DesktopView.css и т.п.).
    emptyOutDir: true,
  },
})

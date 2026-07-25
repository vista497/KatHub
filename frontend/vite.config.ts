import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [vue()],

  server: {
    port: 5173,
    strictPort: false,

    // Proxy /api/* and /ws/* to the C++ backend
    proxy: {
      '/api': {
        target: 'http://localhost:8080',
        changeOrigin: true,
        // WebSocket proxy: upgrade /ws connections as well
        ws: false,  // /ws handled separately below
        secure: false,
      },
      '/ws': {
        target: 'http://localhost:8080',
        changeOrigin: true,
        ws: true,   // enable WebSocket upgrade for /ws
        secure: false,
      },
    },

    // CORS headers for development — allows the Vite dev server
    // to accept cross-origin requests from the backend and tools.
    cors: {
      origin: '*',
      methods: ['GET', 'POST', 'PUT', 'DELETE', 'OPTIONS'],
      allowedHeaders: ['Content-Type', 'Authorization'],
    },
  },
})

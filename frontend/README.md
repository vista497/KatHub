# KatHub Frontend

Vue 3 + TypeScript + Vite — клиентская часть KatHub.

## Dev Server Proxy

При разработке (`npm run dev`) Vite dev server поднимается на `http://localhost:5173` и проксирует запросы к C++ backend:

| Путь       | Цель                     | WebSocket |
|------------|--------------------------|-----------|
| `/api/*`   | `http://localhost:8080`  | нет       |
| `/ws/*`    | `http://localhost:8080`  | да        |

Конфигурация — в `vite.config.ts`, секция `server.proxy`.

### CORS (Development)

Dev server принимает кросс-доменные запросы от любого источника (`origin: *`), методы: `GET`, `POST`, `PUT`, `DELETE`, `OPTIONS`, заголовки: `Content-Type`, `Authorization`.

### Как это работает

1. Запусти C++ backend (по умолчанию слушает порт 8080).
2. Запусти Vite dev server: `npm run dev`.
3. Фронтенд шлёт запросы на `/api/...` и `/ws/...` — Vite прозрачно форвардит их на backend, подменяя `Origin`.

### Сборка

```bash
npm install
npm run dev      # dev-сервер с hot reload (порт 5173)
npm run build    # production-сборка в dist/
npm run preview  # предпросмотр production-сборки
```

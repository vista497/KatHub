# План панелей KatHub (Phase 2 → Dashboard)

## 1. Панель «Cron» — управление задачами
- **API:** Прокси к Hermes API (`/api/cron`): list, get, create, pause/resume, delete, run now
- **Backend:** `CronHandler` (httplib handler, вызывает `HermesApiClient`)
- **Frontend:** `CronPanel.vue` — таблица: название, расписание, статус, последний запуск
- **UI:** Кнопки: ▶ Запустить сейчас, ⏸ Пауза, ✕ Удалить, + Создать
- **Создание:** форма: название, cron-выражение, prompt, скиллы, модель

## 2. Панель «Skills» — скиллы агентов
- **API:** Прокси к Hermes API (`/api/skills`): list, get, create, update, delete
- **Backend:** `SkillsHandler`
- **Frontend:** `SkillsPanel.vue` — список с поиском, просмотр markdown, редактирование
- **UI:** Слева список → справа содержимое (markdown с подсветкой)
- **Создание:** форма: имя, категория, markdown-тело

## 3. Панель «Models» — выбор модели
- **API:** Чтение из Hermes API + config.yaml
- **Backend:** `ModelsHandler` — список доступных моделей, текущая модель, переключение
- **Frontend:** `ModelsPanel.vue` — выпадающий список провайдеров + моделей
- **UI:** Карточка текущей модели, селектор, кнопка «Применить»

## 4. Панель «System» — статус Hermes
- **API:** `/health`, `/api/status` (новая)
- **Backend:** `SystemHandler` — агрегирует: API-статус, модель, версия, порты, uptime
- **Frontend:** `SystemPanel.vue` — карточки: 🟢/🔴 API, модель, версия, порты
- **UI:** Автообновление каждые 10 секунд

## 5. Панель «Agents» — профили агентов
- **API:** Hermes API (`/api/profiles`): list, status, start/stop
- **Backend:** `AgentsHandler`
- **Frontend:** `AgentsPanel.vue` — карточки профилей: имя, статус, модель
- **UI:** Для каждого: 🟢 running / 🔴 stopped, модель, кнопка переключения

---

## Файлы для создания/изменения

### Backend (новые handlers):
- `backend/handlers/CronHandler.cpp/.h`
- `backend/handlers/SkillsHandler.cpp/.h`
- `backend/handlers/ModelsHandler.cpp/.h`
- `backend/handlers/SystemHandler.cpp/.h`
- `backend/handlers/AgentsHandler.cpp/.h`

### Backend (доработка):
- `backend/handlers/HermesApiClient.cpp/.h` — добавить методы для новых эндпоинтов
- `backend/transport/HttpServer.cpp` — зарегистрировать новые роуты
- `build/backend/handlers/kathub-handlers.vcxproj` — добавить новые файлы

### Frontend (новые):
- `frontend/src/views/CronPanel.vue`
- `frontend/src/views/SkillsPanel.vue`
- `frontend/src/views/ModelsPanel.vue`
- `frontend/src/views/SystemPanel.vue`
- `frontend/src/views/AgentsPanel.vue`

### Frontend (доработка):
- `frontend/src/views/DesktopView.vue` — табы для панелей
- `frontend/src/components/layout/SidebarPanel.vue` — ссылки на новые панели
- `frontend/src/router/index.ts` — роуты

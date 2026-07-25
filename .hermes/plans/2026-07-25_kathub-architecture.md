# KatHub — Архитектурный план

> **Для Hermes:** использовать plan skill, внедрять по задачам через под-агентов.
> **Для Мишки:** этот документ — карта проекта. Всё, что нужно знать перед первой строкой кода.

**Цель:** десктоп-приложение на Qt с веб-интерфейсом (Vue 3). Один бинарник работает в двух режимах: сервер (поднимает API + WebSocket и отдаёт UI) и рука (WebEngineView, коннектится к серверу). Основной хост на ПК/сервере, множество рук на разных устройствах.

**Архитектура:** Ports & Adapters (гексагональная). C++ ядро с чистыми интерфейсами, DI через Composition Root, self-update pipeline. `backend/` (C++/Qt) разделён на слои: Domain → Application → Transport → Infrastructure. `frontend/` (Vue 3/Vite) общается с бэкендом через REST + WebSocket.

**Стек:** Qt 6.7.3 (MSVC 2022, C++20), cpp-httplib (REST), Qt WebSockets (WS), Vue 3 + Vite (фронтенд), Qt WebEngine (рука), Inno Setup (инсталлятор).

---

## 0. ДО УСТАНОВКИ: что нужно доустановить

| Компонент | Статус | Действие |
|-----------|--------|----------|
| Qt 6.7.3 MSVC 2022 | ✅ есть | — |
| Qt WebSockets | ✅ есть | — |
| Qt WebEngine | ❌ нет | Установить через MaintenanceTool → Add Components → Qt 6.7.3 → Qt WebEngine |
| QtHttpServer | ❌ нет | **Не ставим.** Используем cpp-httplib (header-only, MIT) |
| Node.js 22.x | ✅ v22.22.3 | — |
| npm 10.x | ✅ 10.9.8 | — |
| CMake | ✅ есть | — |

---

## 1. СТРУКТУРА МОНОРЕПОЗИТОРИЯ

```
kathub/
├── .env.example              # Шаблон для API-ключей
├── .gitignore
├── README.md
├── CMakeLists.txt            # Корневой — включает backend/
│
├── backend/
│   ├── CMakeLists.txt
│   ├── main.cpp              # Точка входа, парсинг --server
│   ├── version.h.in          # Версия из CMake → C++
│   ├── KatHubApp.h/cpp       # QApplication + инициализация
│   │
│   ├── core/                 # Бизнес-логика (из AI_ControllerApp)
│   │   ├── CMakeLists.txt
│   │   ├── AppPaths.h/cpp    # Пути (портировано)
│   │   ├── SettingsRegistry.h/cpp  # Настройки (портировано)
│   │   ├── SignalHub.h/cpp   # Сигнальный хаб (портировано)
│   │   └── ...
│   │
│   ├── server/               # HTTP + WebSocket сервер
│   │   ├── CMakeLists.txt
│   │   ├── HttpServer.h/cpp      # cpp-httplib обёртка
│   │   ├── WsServer.h/cpp        # Qt WebSocket сервер
│   │   ├── Router.h/cpp          # Роутинг REST-запросов
│   │   ├── StaticFileHandler.h/cpp  # Отдача frontend/dist/
│   │   └── handlers/             # Обработчики API
│   │       ├── StatusHandler.h/cpp
│   │       ├── ConfigHandler.h/cpp
│   │       ├── LogHandler.h/cpp
│   │       └── ...
│   │
│   ├── client/               # WebEngine-рука
│   │   ├── CMakeLists.txt
│   │   ├── WebViewWindow.h/cpp   # Окно с QWebEngineView
│   │   └── ClientConfig.h/cpp    # Адрес сервера, порт
│   │
│   ├── shared/               # Общее между server и client
│   │   ├── CMakeLists.txt
│   │   ├── Constants.h           # Порты, endpoints
│   │   ├── ApiTypes.h            # DTO-структуры (JSON-сериализация)
│   │   └── JsonUtils.h/cpp       # QJsonDocument хелперы
│   │
│   └── thirdparty/           # Сторонние зависимости
│       └── cpp-httplib/          # header-only HTTP (MIT)
│
├── frontend/                 # Vue 3 + Vite
│   ├── package.json
│   ├── vite.config.ts
│   ├── index.html
│   ├── tsconfig.json
│   ├── src/
│   │   ├── main.ts
│   │   ├── App.vue
│   │   ├── router/           # Vue Router
│   │   ├── stores/           # Pinia (стейт)
│   │   ├── composables/      # useWebSocket, useApi
│   │   ├── components/       # UI-компоненты
│   │   ├── views/            # Страницы
│   │   └── assets/           # CSS, картинки
│   └── dist/                 # Сборка (игнорируется git,
│                             #   бэкенд отдаёт из этого каталога)
│
├── installer/                # Inno Setup
│   ├── kathub.iss
│   └── assets/
│
└── docs/
    └── architecture.md
```

---

## 2. АРХИТЕКТУРА КОМПОНЕНТОВ

```
┌─────────────────────────────────────────────────────────────┐
│                        KatHub Binary                         │
│                                                              │
│  ┌──────────────────────┐    ┌──────────────────────────┐   │
│  │   MODE: --server     │    │   MODE: (default)        │   │
│  │                      │    │                          │   │
│  │  HttpServer :8080    │    │  WebViewWindow           │   │
│  │  ┌────────────────┐  │    │  ┌────────────────────┐  │   │
│  │  │ REST API       │  │    │  │ QWebEngineView    │  │   │
│  │  │ /api/status    │  │    │  │ → http://HOST:8080│  │   │
│  │  │ /api/config    │  │    │  └────────────────────┘  │   │
│  │  │ /api/...       │  │    │                          │   │
│  │  └────────────────┘  │    └──────────────────────────┘   │
│  │  WsServer :8081      │                                    │
│  │  ┌────────────────┐  │                                    │
│  │  │ WebSocket      │◄─┼──── streaming (AI, logs, etc.)    │
│  │  │ /ws            │  │                                    │
│  │  └────────────────┘  │                                    │
│  │  StaticFiles          │                                    │
│  │  ┌────────────────┐  │                                    │
│  │  │ frontend/dist/ │  │                                    │
│  │  └────────────────┘  │                                    │
│  └──────────────────────┘                                    │
└─────────────────────────────────────────────────────────────┘

   Устройства:
   ┌──────────┐  ┌──────────┐  ┌──────────┐
   │  ПК №1   │  │ Телефон  │  │ Планшет  │
   │ WebEngine│  │ Браузер  │  │ Браузер  │
   │   Рука   │  │  (WiFi)  │  │  (WiFi)  │
   └────┬─────┘  └────┬─────┘  └────┬─────┘
        │              │              │
        └──────────────┼──────────────┘
                       │
               http://HOST:8080
                       │
               ┌───────┴───────┐
               │   KatHub      │
               │   --server    │
               │   (ПК/сервер) │
               └───────────────┘
```

---

## 3. ПОТОКИ И ПРОТОКОЛЫ

### REST API (порт 8080 по умолчанию)
| Метод | Путь | Назначение |
|-------|------|-----------|
| GET | `/api/status` | Статус сервера, версия |
| GET | `/api/config` | Текущая конфигурация |
| PUT | `/api/config` | Обновление конфигурации |
| GET | `/api/logs` | Последние N записей лога |
| GET | `/api/sessions` | Список сессий |
| POST | `/api/sessions` | Создать сессию |
| GET | `/api/sessions/:id` | Детали сессии |
| ... | ... | Расширяется по мере разработки |

### WebSocket (порт 8081 по умолчанию)
| Событие | Направление | Назначение |
|---------|-------------|-----------|
| `stream:chunk` | Server → Client | Фрагмент ответа AI |
| `stream:done` | Server → Client | Конец стрима |
| `log:entry` | Server → Client | Запись лога |
| `notify:*` | Server → Client | Уведомления |
| `cmd:exec` | Client → Server | Команда от руки к ядру |

### Запуск руки
```
1. WebEngineView загружает http://localhost:8080 (по умолчанию)
2. Vue-приложение в браузере устанавливает WebSocket к ws://localhost:8081
3. Обмен: REST для команд, WS для real-time данных
```

---

## 4. ФАЗЫ РАЗРАБОТКИ

### Фаза 1: Скелет и HTTP-сервер
- Монорепо + CMake
- cpp-httplib: HTTP-сервер на 8080
- `GET /api/status` → `{"status": "ok", "version": "0.1.0"}`
- Статическая раздача `frontend/dist/`
- Интеграционный тест: `curl localhost:8080/api/status`

### Фаза 2: WebSocket и real-time
- Qt WebSocket сервер на 8081
- Ping/pong протокол
- Хаб событий: SignalHub → WS broadcast
- Тест: `wscat -c ws://localhost:8081/ws`

### Фаза 3: Фронтенд (Vue 3)
- Инициализация Vue 3 + Vite + TypeScript
- Базовый лэйаут: сайдбар + основная область
- Подключение к REST API → отображение статуса
- WebSocket composable
- Горячая перезагрузка через Vite dev proxy

### Фаза 4: WebEngine-рука
- Установка Qt WebEngine через MaintenanceTool
- WebViewWindow: frameless окно с QWebEngineView
- Аргументы: `--host`, `--port` для подключения к удалённому серверу
- Автоопределение: если localhost:8080 отвечает → подключается, иначе запускает сервер

### Фаза 5: Портирование ядра из AI_ControllerApp
- AppPaths, SettingsRegistry, SignalHub
- AI-бэкенды (OpenRouter API)
- Логгер
- PythonEnvironment (по необходимости)

### Фаза 6: Инсталлятор и деплой
- Inno Setup скрипт
- Упаковка frontend/dist/ + бинарник + Qt DLL
- Автозапуск сервера при старте Windows (опционально)

---

## 5. КЛЮЧЕВЫЕ РЕШЕНИЯ И ОБОСНОВАНИЯ

| Решение | Почему |
|---------|--------|
| `cpp-httplib` вместо QtHttpServer | QtHttpServer не установлен, ставить отдельно. cpp-httplib — один .h файл, MIT, миллион звёзд на GitHub. Проще, быстрее, никаких зависимостей. |
| Qt WebSockets (а не socket.io) | Уже установлен, нативный C++, не тянет Node.js в бэкенд |
| Vue 3, не React | Ниже порог входа, SFC похожи на QML, доки на русском |
| Vite, не Webpack | Мгновенная сборка, нативный ESM, меньше конфигов |
| TypeScript на фронтенде | Ловит опечатки на этапе сборки. Для не-JS-разработчика критично |
| Pinia для стейта | Официальный стор Vue 3, проще Vuex в разы |
| `.env` + `.env.example` | Стандарт индустрии, Qt-бэкенд читает QSettings или вручную |

---

## 6. РИСКИ И ОТКРЫТЫЕ ВОПРОСЫ

| Риск | Вероятность | Митигация |
|------|------------|-----------|
| WebEngine нестабилен в production | Средняя | Можно переключить руку на системный WebView2 (Windows) |
| Vue для новичка — долгий старт | Высокая | Фаза 3 начинается с минимального «Hello, KatHub», постепенно усложняем |
| Синхронизация стейта между руками | Высокая | WS broadcast всем подключённым клиентам |
| cpp-httplib не тянет нагрузку | Низкая | Меняем на QtHttpServer, когда доустановим |

### Открытые вопросы:
1. **Аутентификация** — решено: Tailscale mesh VPN, внутри сети без дополнительной защиты. Используем MagicDNS (`kathub` → IP хоста).
2. **mDNS/Bonjour** — не нужно, Tailscale DNS покрывает автообнаружение.
3. **HTTPS** — не нужно внутри Tailscale-сети (трафик уже зашифрован WireGuard).
4. **Портировать AI-бэкенды сразу или начать с заглушек?** Рекомендую заглушки — быстрее рабочий прототип.

---

## 7. ПЕРВЫЙ ЗАПУСК (после Фазы 1)

```bash
# Сервер
./kathub --server
# → HTTP на :8080, WS на :8081
# → http://localhost:8080/ — Vue-приложение

# Рука
./kathub
# → WebEngineView открывает localhost:8080

# Рука на другом ПК
./kathub --host 192.168.1.100
# → WebEngineView открывает http://192.168.1.100:8080
```

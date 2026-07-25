# KatHub

**Десктопный AI-хаб с веб-интерфейсом.** Собственный ChatGPT-подобный клиент, работающий через Tailscale VPN без открытых портов. Один бинарник — два режима: сервер (API + WebSocket) и рука (окно WebEngine).

> Статус: v0.6.0 — все компоненты реализованы, тесты проходят, инсталлятор собирается.

## Архитектура

```
┌──────────────────────────────────────────────┐
│                 KatHub Binary                 │
│                                               │
│   --server              (default)             │
│   HttpServer :8080      WebEngineView         │
│   WsServer  :8081       → http://HOST:8080   │
│   AIController                                │
│   Plugin System (DLL)                         │
└──────────────────────────────────────────────┘
          │
    ┌─────┴─────┐
    │ Tailscale │  mesh VPN, MagicDNS
    └─────┬─────┘
          │
    Устройства: ПК, телефон, планшет
```

**Паттерн:** Ports & Adapters (гексагональная). Чистые интерфейсы в `backend/domain/`, реализация в `backend/core/`, `backend/transport/`, `backend/host/`. DI через Composition Root (`KatHubApp`).

## Стек

| Слой | Технологии |
|------|-----------|
| Ядро | C++20, Qt 6.7.3 (MSVC 2022) |
| HTTP | cpp-httplib (header-only, MIT) |
| WebSocket | Qt WebSockets |
| AI | OpenRouter API, OpenAI-совместимые бэкенды |
| Фронтенд | Vue 3 + Vite + TypeScript |
| Рука | Qt WebEngine |
| Сеть | Tailscale mesh VPN (WireGuard) |
| Инсталлятор | Inno Setup |

## Быстрый старт

### Требования

- Qt 6.7.3 (MSVC 2022) — `C:\Qt_new\6.7.3\msvc2022_64`
- MSVC 2022 BuildTools
- CMake 3.20+
- Node.js 22+ (для фронтенда)
- Tailscale (для сетевого доступа)

### Сборка

```bash
# Клонирование
git clone https://github.com/vista497/KatHub.git
cd KatHub

# Бэкенд
cmake -B build -DCMAKE_PREFIX_PATH=C:/Qt_new/6.7.3/msvc2022_64
cmake --build build --config Debug

# Тесты
ctest --test-dir build -C Debug

# Фронтенд (опционально — production static уже в backend/static/)
cd frontend
npm install
npm run dev      # Dev-сервер с hot-reload на :5173
npm run build    # Сборка в dist/
```

### Запуск

```bash
# Сервер (API + WebSocket + статика)
./build/backend/Debug/kathub-backend.exe --server

# → HTTP  : http://localhost:8080
# → WS    : ws://localhost:8081/ws
# → Статус: http://localhost:8080/api/status

# Рука (WebEngine окно)
./build/backend/Debug/kathub-backend.exe --hand

# Рука с подключением к удалённому серверу
./build/backend/Debug/kathub-backend.exe --hand --host my-server
```

### API

| Метод | Путь | Описание |
|-------|------|----------|
| `GET` | `/api/status` | Статус сервера, версия, uptime |
| `GET` | `/api/plugins` | Список загруженных плагинов |
| `GET` | `/api/config` | Текущая конфигурация |
| `PUT` | `/api/config` | Обновление конфигурации |
| `POST` | `/api/chat` | Отправка сообщения (SSE streaming) |
| `WS` | `/ws` | WebSocket: события, стриминг, логи |

### WebSocket события

| Событие | Направление | Назначение |
|---------|-------------|-----------|
| `stream:chunk` | Server → Client | Фрагмент ответа AI |
| `stream:done` | Server → Client | Конец стрима |
| `log:entry` | Server → Client | Запись лога |
| `notify:*` | Server → Client | Уведомления |

## Структура проекта

```
KatHub/
├── CMakeLists.txt            # Корневой CMake
├── backend/                  # C++ ядро
│   ├── main.cpp              # Точка входа
│   ├── KatHubApp.h/cpp       # Composition Root
│   ├── ai/                   # AI-контроллер
│   │   ├── AIController      # Управление AI-запросами
│   │   ├── Conversation      # История диалога
│   │   ├── OpenRouterClient  # OpenRouter API
│   │   └── ToolDispatcher    # Вызов инструментов
│   ├── config/               # Загрузка конфигурации
│   │   └── JsonConfigLoader  # .json + .env
│   ├── core/                 # Бизнес-логика
│   │   ├── AIService         # Управление бэкендами
│   │   ├── AppPaths          # Пути приложения
│   │   ├── Logger            # Логирование
│   │   ├── SettingsRegistry  # Настройки
│   │   └── SignalHub         # Сигнальный хаб (pub/sub)
│   ├── domain/               # Интерфейсы
│   │   ├── IPlugin.h         # Интерфейс плагина
│   │   ├── IHttpHandler.h    # Интерфейс HTTP-обработчика
│   │   ├── IEventBus.h       # Интерфейс шины событий
│   │   ├── HostApi.h         # API для плагинов
│   │   └── ConfigService     # Сервис конфигурации
│   ├── handlers/             # HTTP-обработчики
│   │   ├── StatusHandler     # GET /api/status
│   │   ├── PluginListHandler # GET /api/plugins
│   │   ├── ChatHandler       # POST /api/chat (SSE)
│   │   └── WsStatusHandler   # WS /ws обработчик
│   ├── host/                 # Инфраструктура
│   │   ├── PluginLoader      # Загрузчик DLL-плагинов
│   │   ├── PluginRegistry    # Реестр плагинов
│   │   └── BackendLoader     # Загрузка AI-бэкендов
│   ├── prompts/              # Промпты и профили
│   │   ├── AgentProfile      # Профиль агента
│   │   ├── PromptManager     # Менеджер шаблонов
│   │   └── templates/        # .md шаблоны
│   ├── transport/            # Сетевой слой
│   │   ├── HttpServer        # HTTP (cpp-httplib)
│   │   ├── WsServer          # WebSocket (Qt)
│   │   └── StaticFileHandler # Раздача статики
│   ├── ui/                   # GUI
│   │   ├── HandWindow        # Окно с WebEngine
│   │   └── WebEngineStub     # Заглушка без WebEngine
│   └── static/               # Собранный фронтенд
├── frontend/                 # Vue 3 приложение
│   ├── src/
│   │   ├── App.vue
│   │   ├── main.ts
│   │   └── components/
│   └── vite.config.ts
├── tests/                    # C++ тесты (Google Test)
│   ├── test_plugin_lifecycle.cpp
│   ├── test_status_endpoint.cpp
│   ├── test_eventbus.cpp
│   ├── test_websocket.cpp
│   └── test_ui.html          # HTML-дашборд
├── installer/                # Inno Setup
│   ├── kathub.iss
│   └── package_staging.py
└── .hermes/plans/            # Планы архитектуры
```

## Плагинная система

Плагины — DLL с `extern "C"` фабрикой. Загружаются из `plugins/` при старте сервера.

```cpp
// Интерфейс плагина (domain/IPlugin.h)
class IPlugin {
public:
    virtual const char* name() const = 0;
    virtual const char* version() const = 0;
    virtual bool initialize(const HostApi& api) = 0;
    virtual void shutdown() = 0;
};

// Экспорт из DLL
extern "C" KATHUB_DOMAIN_EXPORT IPlugin* createPlugin();
```

Плагины получают `HostApi` с непрозрачными указателями на Router, EventBus, ConfigService, Logger.

## AI-бэкенды

Декларативная конфигурация в `providers.json`:

```json
{
  "providers": [
    {
      "name": "openrouter",
      "type": "openai_compatible",
      "baseUrl": "https://openrouter.ai/api/v1",
      "apiKeyEnv": "OPENROUTER_API_KEY",
      "models": ["anthropic/claude-sonnet-4", "openai/gpt-4o"]
    }
  ]
}
```

## Самообновление

Pipeline: Check → Build → Relaunch. Новый процесс должен ответить на `/api/status` в течение 30 секунд перед тем как старый будет убит.

## Разработка

```bash
# Запуск тестов
ctest --test-dir build -C Debug

# Только бэкенд
cmake --build build --config Debug --target kathub-backend

# Фронтенд dev-сервер
cd frontend && npm run dev
# Проксирует /api/* на localhost:8080
```

## Лицензия

MIT

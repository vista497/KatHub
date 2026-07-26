add_library(kathub-handlers STATIC)

target_sources(kathub-handlers PRIVATE
    StatusHandler.h StatusHandler.cpp
    WsStatusHandler.h WsStatusHandler.cpp
    PluginListHandler.h PluginListHandler.cpp
    ChatHandler.h ChatHandler.cpp
    VaultGraphHandler.h VaultGraphHandler.cpp
    VaultFileHandler.h VaultFileHandler.cpp
    VaultSessionsHandler.h VaultSessionsHandler.cpp
    HermesApiClient.h HermesApiClient.cpp
    HermesSessionsHandler.h HermesSessionsHandler.cpp
    HermesCliHelper.h HermesCliHelper.cpp
    ModelsHandler.h ModelsHandler.cpp
    SystemHandler.h SystemHandler.cpp
    AgentsHandler.h AgentsHandler.cpp
    CronHandler.h CronHandler.cpp
    SkillsHandler.h SkillsHandler.cpp
    KanbanHandler.h KanbanHandler.cpp
)

target_include_directories(kathub-handlers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/backend/domain
    ${CMAKE_SOURCE_DIR}/backend/host ${CMAKE_SOURCE_DIR}/backend/core
    ${CMAKE_SOURCE_DIR}/backend/transport ${CMAKE_SOURCE_DIR}/backend/ai
    ${CMAKE_SOURCE_DIR}/backend/prompts ${CMAKE_SOURCE_DIR}/backend/thirdparty/cpp-httplib
)

target_link_libraries(kathub-handlers PUBLIC
    kathub-domain kathub-host kathub-core kathub-ai kathub-prompts Qt6::Core
)

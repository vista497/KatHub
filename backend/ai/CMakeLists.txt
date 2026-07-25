add_library(kathub-ai STATIC)

target_sources(kathub-ai PRIVATE
    OpenRouterClient.h
    OpenRouterClient.cpp
    Conversation.h
    Conversation.cpp
    ToolDispatcher.h
    ToolDispatcher.cpp
    AIController.h
    AIController.cpp
)

target_include_directories(kathub-ai PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/domain/include
)

target_include_directories(kathub-ai PRIVATE
    ${CMAKE_SOURCE_DIR}/backend/thirdparty/cpp-httplib
    ${CMAKE_SOURCE_DIR}/backend
)

target_link_libraries(kathub-ai PUBLIC
    kathub-domain
    Qt6::Core
)

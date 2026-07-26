add_library(kathub-host STATIC)

target_sources(kathub-host PRIVATE
    PluginRegistry.h
    PluginRegistry.cpp
    PluginLoader.h
    PluginLoader.cpp
    BackendLoader.h
    BackendLoader.cpp
)

target_include_directories(kathub-host PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(kathub-host PRIVATE
    ${CMAKE_SOURCE_DIR}/backend/domain
    ${CMAKE_SOURCE_DIR}/libs/domain/include
)

target_link_libraries(kathub-host PUBLIC
    kathub-domain
    kathub-core
    kathub-ai
    Qt6::Core
)

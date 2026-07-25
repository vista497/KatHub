add_library(kathub-host STATIC)

target_sources(kathub-host PRIVATE
    PluginRegistry.h
    PluginRegistry.cpp
    PluginLoader.h
    PluginLoader.cpp
)

target_include_directories(kathub-host PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/eventbus/include
)
target_include_directories(kathub-host PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/domain/include
    ${CMAKE_SOURCE_DIR}/libs/eventbus/include
)

target_link_libraries(kathub-host PUBLIC
    kathub-domain
    kathub-core
    kathub-eventbus
    Qt6::Core
)

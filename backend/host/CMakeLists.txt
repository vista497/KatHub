add_library(kathub-host STATIC)

target_sources(kathub-host PRIVATE
    PluginRegistry.h
    PluginRegistry.cpp
    PluginLoader.h
    PluginLoader.cpp
)

target_include_directories(kathub-host PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(kathub-host PRIVATE
    ${CMAKE_SOURCE_DIR}/backend/domain
)

target_link_libraries(kathub-host PUBLIC
    kathub-domain
    kathub-core
    Qt6::Core
)

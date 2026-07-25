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

target_link_libraries(kathub-host PUBLIC
    kathub-domain
    Qt6::Core
)

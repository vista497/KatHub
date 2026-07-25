cmake_minimum_required(VERSION 3.20)

# Build a minimal plugin DLL to verify the plugin loading machinery.
# Defines KATHUB_DOMAIN_BUILD_DLL so that IPlugin symbols are exported
# from this DLL rather than imported.

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(test_plugin SHARED
    test_plugin.h
    test_plugin.cpp
)

target_include_directories(test_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/backend/domain
)

target_compile_definitions(test_plugin PRIVATE
    KATHUB_DOMAIN_BUILD_DLL
)

target_link_libraries(test_plugin PRIVATE
    kathub-domain
    Qt6::Core
)

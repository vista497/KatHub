find_package(Qt6 REQUIRED COMPONENTS Core WebSockets Test Network)

add_executable(kathub-tests
    test_plugin_lifecycle.cpp
    test_status_endpoint.cpp
    test_websocket.cpp
)

# Include cpp-httplib header for the status endpoint test
target_include_directories(kathub-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/backend/thirdparty/cpp-httplib
)

# Pass the absolute path to the test plugin DLL via a compile definition.
# $<TARGET_FILE:test_plugin> expands to the full DLL path at build time,
# including the per-config subdirectory (Debug/Release/etc.).
target_compile_definitions(kathub-tests PRIVATE
    "TEST_PLUGIN_DLL_PATH=\"$<TARGET_FILE:test_plugin>\""
)

target_link_libraries(kathub-tests PRIVATE
    kathub-domain
    kathub-host
    kathub-transport
    kathub-handlers
    kathub-core
    gtest
    Qt6::Core
    Qt6::WebSockets
    Qt6::Network
    Qt6::Test
)

# MSVC discards static lib objects with no referenced symbols.
# REGISTER_HANDLER uses file-scope globals — force linker to keep them.
if(MSVC)
    target_link_options(kathub-tests PRIVATE "/WHOLEARCHIVE:kathub-handlers")
endif()

# cpp-httplib needs Winsock2 on Windows
if(WIN32)
    target_link_libraries(kathub-tests PRIVATE ws2_32)
endif()

# ---------------------------------------------------------------------------
# CTest integration
# We use add_test + ENVIRONMENT_MODIFICATION instead of gtest_discover_tests
# because gtest_discover_tests runs the executable at *build* time (to list
# test cases), and that step needs Qt DLLs on PATH.  add_test only runs at
# ctest time where the user controls the environment.
# ---------------------------------------------------------------------------

# Derive Qt binary directory for runtime PATH
get_filename_component(_qt_prefix "${Qt6_DIR}/../.." ABSOLUTE)
set(_qt_bin "${_qt_prefix}/bin")

add_test(NAME kathub-tests COMMAND kathub-tests)
set_property(TEST kathub-tests PROPERTY
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}"
)

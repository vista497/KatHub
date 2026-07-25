find_package(Qt6 REQUIRED COMPONENTS Core WebSockets Test Network)

add_executable(kathub-tests
    test_eventbus.cpp
    test_plugin_lifecycle.cpp
    test_status_endpoint.cpp
    test_websocket.cpp
    config/test_config_service.cpp
)

# Include directories
target_include_directories(kathub-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/backend/thirdparty/cpp-httplib
    ${googletest_SOURCE_DIR}/googlemock/include
    ${googletest_SOURCE_DIR}/googletest/include
)

# Pass the absolute path to the test plugin DLL
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

# Standalone ConfigService test (domain/ConfigService.h)
add_executable(config-service-test
    config/test_config_service.cpp
)
target_include_directories(config-service-test PRIVATE
    ${CMAKE_SOURCE_DIR}/backend/domain
    ${googletest_SOURCE_DIR}/googletest/include
)
target_link_libraries(config-service-test PRIVATE
    kathub-domain
    gtest
    gtest_main
    Qt6::Core
)
if(MSVC)
    target_compile_options(config-service-test PRIVATE "/FS")
endif()

if(MSVC)
    target_link_options(kathub-tests PRIVATE "/WHOLEARCHIVE:kathub-handlers")
    target_compile_options(kathub-tests PRIVATE "/FS")
endif()

if(WIN32)
    target_link_libraries(kathub-tests PRIVATE ws2_32)
endif()

# CTest
get_filename_component(_qt_prefix "${Qt6_DIR}/../.." ABSOLUTE)
set(_qt_bin "${_qt_prefix}/bin")

add_test(NAME kathub-tests COMMAND kathub-tests)
set_property(TEST kathub-tests PROPERTY
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}"
)

add_test(NAME config-service-test COMMAND config-service-test)
set_property(TEST config-service-test PROPERTY
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}"
)

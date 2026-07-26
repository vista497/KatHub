cmake_minimum_required(VERSION 3.22)
project(json_config_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use same gtest from parent build
set(GTEST_ROOT "${CMAKE_SOURCE_DIR}/../../build2/_deps/googletest-src")
find_package(GTest REQUIRED)

add_executable(json_config_test
    ../test_json_standalone.cpp
)

target_include_directories(json_config_test PRIVATE
    ${CMAKE_SOURCE_DIR}/../../libs/config/include
)

target_link_libraries(json_config_test PRIVATE
    GTest::gtest
    ${CMAKE_SOURCE_DIR}/../../build2/libs/config/Debug/kathub-json-config.lib
)

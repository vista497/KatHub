cmake_minimum_required(VERSION 3.20)
project(KatHub VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt integration
find_package(Qt6 REQUIRED COMPONENTS Core WebSockets)
find_package(Qt6 QUIET COMPONENTS WebEngineWidgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# MSVC parallel builds: /FS serialises PDB writes to prevent
# "fatal error C1041: cannot open program database" races.
if(MSVC)
    add_compile_options(/FS)
endif()

# nlohmann/json — header-only, bundled in libs/config/include/nlohmann/
add_library(nlohmann_json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
target_include_directories(nlohmann_json INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/config/include
)

# Google Test via FetchContent
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_subdirectory(libs/domain)
add_subdirectory(libs/eventbus)
add_subdirectory(libs/ui)
add_subdirectory(libs/transport)
add_subdirectory(libs/config)
add_subdirectory(libs/backend)
add_subdirectory(backend)
add_subdirectory(tests/test_plugin)
add_subdirectory(tests)

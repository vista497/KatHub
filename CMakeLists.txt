cmake_minimum_required(VERSION 3.20)
project(KatHub VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt integration
find_package(Qt6 REQUIRED COMPONENTS Core Widgets WebSockets WebEngineWidgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# MSVC parallel builds: /FS serialises PDB writes to prevent
# "fatal error C1041: cannot open program database" races.
if(MSVC)
    add_compile_options(/FS)
endif()

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

add_subdirectory(backend)
add_subdirectory(tests/test_plugin)
add_subdirectory(tests)

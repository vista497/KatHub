find_package(Qt6 REQUIRED COMPONENTS Core WebEngineWidgets)

add_library(kathub-ui STATIC)

target_sources(kathub-ui PRIVATE
    src/WebEngineStub.h
    src/WebEngineStub.cpp
)

target_include_directories(kathub-ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(kathub-ui PUBLIC
    Qt6::Core
    Qt6::WebEngineWidgets
)

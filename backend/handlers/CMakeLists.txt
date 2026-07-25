add_library(kathub-handlers STATIC)

target_sources(kathub-handlers PRIVATE
    StatusHandler.h
    StatusHandler.cpp
)

target_include_directories(kathub-handlers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/backend/domain
    ${CMAKE_SOURCE_DIR}/backend/host
    ${CMAKE_SOURCE_DIR}/backend/core
    ${CMAKE_SOURCE_DIR}/backend/thirdparty/cpp-httplib
)

target_link_libraries(kathub-handlers PUBLIC
    kathub-domain
    kathub-host
    kathub-core
    Qt6::Core
)

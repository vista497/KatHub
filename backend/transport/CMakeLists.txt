add_library(kathub-transport STATIC)

target_sources(kathub-transport PRIVATE
    HttpServer.h
    HttpServer.cpp
    StaticFileHandler.h
    StaticFileHandler.cpp
    WsServer.h
    WsServer.cpp
)

target_include_directories(kathub-transport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/backend/thirdparty/cpp-httplib
    ${CMAKE_SOURCE_DIR}/backend/core
)

target_link_libraries(kathub-transport PUBLIC
    kathub-domain
    kathub-host
    kathub-core
    Qt6::Core
    Qt6::WebSockets
)

# cpp-httplib needs Winsock2 on Windows
if(WIN32)
    target_link_libraries(kathub-transport PRIVATE ws2_32)
endif()

add_library(kathub-transport STATIC)

target_sources(kathub-transport PRIVATE
    HttpServer.h
    HttpServer.cpp
    StaticFileHandler.h
    StaticFileHandler.cpp
)

target_include_directories(kathub-transport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/backend/thirdparty/cpp-httplib
)

target_link_libraries(kathub-transport PUBLIC
    kathub-domain
    kathub-host
    Qt6::Core
)

# cpp-httplib needs Winsock2 on Windows
if(WIN32)
    target_link_libraries(kathub-transport PRIVATE ws2_32)
endif()

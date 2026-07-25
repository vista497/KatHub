add_library(kathub-domain STATIC)

target_sources(kathub-domain PRIVATE
    domain_export.h
    IPlugin.h
    IHttpHandler.h
    HostApi.h
)

target_include_directories(kathub-domain PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kathub-domain PUBLIC
    Qt6::Core
)

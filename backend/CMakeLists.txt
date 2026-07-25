add_subdirectory(domain)
add_subdirectory(config)
add_subdirectory(host)
add_subdirectory(core)
add_subdirectory(transport)
add_subdirectory(handlers)

add_executable(kathub-backend
    main.cpp
    KatHubApp.h
    KatHubApp.cpp
)
target_link_libraries(kathub-backend PRIVATE
    kathub-transport
    kathub-handlers
    kathub-core
    kathub-config
    kathub-ui
    kathub-eventbus
    Qt6::Core
    Qt6::WebEngineWidgets
)
if(MSVC)
    target_link_options(kathub-backend PRIVATE "/WHOLEARCHIVE:kathub-handlers")
endif()

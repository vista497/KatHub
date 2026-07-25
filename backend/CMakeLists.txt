find_package(Qt6 REQUIRED COMPONENTS Core)

add_subdirectory(domain)
add_subdirectory(host)
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
    Qt6::Core
)
# MSVC discards static lib objects with no referenced symbols.
# REGISTER_HANDLER uses file-scope globals — force linker to keep them.
if(MSVC)
    target_link_options(kathub-backend PRIVATE "/WHOLEARCHIVE:kathub-handlers")
endif()

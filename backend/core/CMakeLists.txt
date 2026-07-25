add_library(kathub-core STATIC)

target_sources(kathub-core PRIVATE
    SignalHub.h
    SignalHub.cpp
    AIService.h
    AIService.cpp
)

target_include_directories(kathub-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/domain/include
)

target_link_libraries(kathub-core PUBLIC
    kathub-domain
    Qt6::Core
)

add_library(kathub-ai INTERFACE)

target_sources(kathub-ai INTERFACE
    include/kathub/ai/IBackendProvider.h
)

target_include_directories(kathub-ai INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

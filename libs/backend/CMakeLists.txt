add_library(kathub-backend-loader STATIC)

target_sources(kathub-backend-loader PRIVATE
    include/kathub/backend/BackendLoader.h
    src/BackendLoader.cpp
)

target_include_directories(kathub-backend-loader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/domain/include
)

target_link_libraries(kathub-backend-loader PUBLIC
    kathub-ai
    kathub-domain
    Qt6::Core
)

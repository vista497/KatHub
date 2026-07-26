add_library(kathub-config STATIC)

target_sources(kathub-config PRIVATE
    JsonConfigLoader.h
    JsonConfigLoader.cpp
)

target_include_directories(kathub-config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kathub-config PUBLIC
    Qt6::Core
)

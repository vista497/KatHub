add_library(kathub-json-config STATIC)

target_sources(kathub-json-config PRIVATE
    # Public headers (nlohmann::json-based)
    include/kathub/config/ConfigSchema.h

    # JsonConfigLoader
    src/JsonConfigLoader.cpp
)

target_include_directories(kathub-json-config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(kathub-json-config PUBLIC
    nlohmann_json::nlohmann_json
)

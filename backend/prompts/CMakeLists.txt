add_library(kathub-prompts STATIC)

target_sources(kathub-prompts PRIVATE
    PromptManager.h
    PromptManager.cpp
    AgentProfile.h
    AgentProfile.cpp
)

target_include_directories(kathub-prompts PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kathub-prompts PUBLIC
    Qt6::Core
)

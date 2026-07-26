add_library(kathub-eventbus STATIC)

target_sources(kathub-eventbus PRIVATE
    include/IEventBus.h
    include/EventBus.h
    include/DIContainer.h
    src/EventBus.cpp
)

target_include_directories(kathub-eventbus PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(kathub-eventbus PUBLIC
    Qt6::Core
)

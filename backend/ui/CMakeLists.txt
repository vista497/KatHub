add_library(kathub-ui STATIC
    WebEngineStub.h
    WebEngineStub.cpp
)
target_link_libraries(kathub-ui PUBLIC Qt6::Widgets)
target_include_directories(kathub-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

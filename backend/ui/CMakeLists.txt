add_library(kathub-ui STATIC
    HandWindow.h
    HandWindow.cpp
)
target_link_libraries(kathub-ui PUBLIC Qt6::Widgets Qt6::WebEngineWidgets)
target_include_directories(kathub-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Header-only interface library — no Qt dependency.
# The IWebSocketTransport interface is pure C++20.
# Concrete Qt adapter will be a separate STATIC library that
# links kathub-transport-iface + Qt6::WebSockets.
add_library(kathub-transport-iface INTERFACE)

target_include_directories(kathub-transport-iface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

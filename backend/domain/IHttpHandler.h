#pragma once

// Pure abstract HTTP handler interface.
// Plugins that handle HTTP requests implement this.

class IHttpHandler
{
public:
    enum class HttpMethod
    {
        GET,
        POST,
        PUT,
        DELETE,
        PATCH,
        ALL  // Register for all HTTP methods — handler routes internally
    };

    virtual ~IHttpHandler() = default;

    // URL route pattern this handler matches (e.g. "/api/users").
    virtual const char* route() = 0;

    // HTTP method this handler responds to.
    virtual HttpMethod method() = 0;

    // Process an incoming request.
    // request  — raw request body (JSON, form data, etc.) as a null-terminated C string.
    // response — opaque pointer to the response object; cast to the host's response type.
    virtual void handle(const char* request, void* response) = 0;

    // Extended handle with full request context.
    // Default implementation delegates to handle(body, response) for backward compat.
    // Override this if you need URL path, query parameters, or HTTP method.
    // method — HTTP method string ("GET", "POST", "PUT", "DELETE", "PATCH"), or nullptr
    virtual void handleWithContext(
        const char* body,
        const char* path,     // Full request path (e.g. "/api/vault/file/foo.md")
        const char* query,    // Query string (e.g. "path=foo.md"), or nullptr
        void* response,
        const char* method = nullptr)  // HTTP method string, or nullptr
    {
        (void)path;
        (void)query;
        (void)method;
        handle(body, response);
    }
};

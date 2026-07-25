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
        DELETE
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
};

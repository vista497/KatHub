#pragma once

#include "IHttpHandler.h"

class WsServer;

// Built-in handler: GET /api/ws/status
// Returns JSON with WebSocket connection info:
//   {"status":"ok","port":8081,"clients":2,
//    "subscriptions":["http.request","system.ready"]}
class WsStatusHandler : public IHttpHandler
{
public:
    WsStatusHandler();

    const char* route() override;
    HttpMethod method() override;
    void handle(const char* request, void* response) override;

    /// Set the WsServer for querying connection state.
    static void setWsServer(WsServer *server);

private:
    static WsServer *wsServer_;
};

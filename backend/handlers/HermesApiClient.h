#pragma once

#include <string>

// Simple HTTP client for Hermes Agent API Server (http://127.0.0.1:8642).
// Uses cpp-httplib (header-only) for both server and client operations.
// Authentication: Bearer token required for all endpoints except /health.

class HermesApiClient
{
public:
    HermesApiClient(const std::string& baseUrl, const std::string& apiKey);

    // ── Sessions ────────────────────────────────────────────────────────
    // GET /api/sessions → JSON array of sessions
    std::string listSessions();

    // GET /api/sessions/{id}/messages → JSON array of messages
    std::string getMessages(const std::string& sessionId);

    // POST /api/sessions → create new session, returns {"session_id":"..."}
    std::string createSession();

    // ── Chat ────────────────────────────────────────────────────────────
    // POST /api/sessions/{id}/chat → synchronous chat (blocking)
    // body: {"message": "user text"}
    // returns: JSON with agent reply
    std::string chat(const std::string& sessionId, const std::string& message);

    // Delete session: DELETE /api/sessions/{id}
    std::string deleteSession(const std::string& sessionId);

    // Health check: GET /health
    bool isAlive();

private:
    std::string request(const std::string& method,
                        const std::string& path,
                        const std::string& body = "");

    std::string baseUrl_;
    std::string apiKey_;
};

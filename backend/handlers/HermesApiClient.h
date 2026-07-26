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

    // POST /api/sessions/{id}/chat → synchronous chat (blocking)
    std::string chat(const std::string& sessionId, const std::string& message);

    // DELETE /api/sessions/{id}
    std::string deleteSession(const std::string& sessionId);

    // ── Cron ────────────────────────────────────────────────────────────
    // GET /api/cron → list all cron jobs
    std::string listCron();

    // GET /api/cron/{name} → get single cron job
    std::string getCron(const std::string& name);

    // POST /api/cron → create new cron job; body is JSON config
    std::string createCron(const std::string& config);

    // DELETE /api/cron/{name}
    std::string deleteCron(const std::string& name);

    // PATCH /api/cron/{name}/toggle → toggle enabled/disabled
    std::string toggleCron(const std::string& name);

    // POST /api/cron/{name}/run → run cron job immediately
    std::string runCron(const std::string& name);

    // ── Skills ──────────────────────────────────────────────────────────
    // GET /api/skills → list all skills
    std::string listSkills();

    // GET /api/skills/{name} → get single skill
    std::string getSkill(const std::string& name);

    // POST /api/skills → create new skill; body is JSON config
    std::string createSkill(const std::string& config);

    // PUT /api/skills/{name} → update skill; body is JSON config
    std::string updateSkill(const std::string& name, const std::string& config);

    // DELETE /api/skills/{name}
    std::string deleteSkill(const std::string& name);

    // ── Models ──────────────────────────────────────────────────────────
    // GET /api/models → list available models
    std::string listModels();

    // GET /api/models/{name} → get single model info
    std::string getModel(const std::string& name);

    // POST /api/models/switch → switch active model; body: {"model":"..."}
    std::string switchModel(const std::string& modelName);

    // ── System ──────────────────────────────────────────────────────────
    // GET /api/system/status → system health + stats
    std::string getSystemStatus();

    // ── Profiles ────────────────────────────────────────────────────────
    // GET /api/profiles → list all profiles
    std::string listProfiles();

    // GET /api/profiles/{name}/status → profile status
    std::string getProfileStatus(const std::string& profileName);

    // ── Health ──────────────────────────────────────────────────────────
    // GET /health
    bool isAlive();

private:
    std::string request(const std::string& method,
                        const std::string& path,
                        const std::string& body = "");

    std::string baseUrl_;
    std::string apiKey_;
};

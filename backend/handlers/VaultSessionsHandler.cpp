#include "VaultSessionsHandler.h"

#include "httplib.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include <regex>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
const char* VaultSessionsHandler::route() { return "/api/vault/sessions"; }

IHttpHandler::HttpMethod VaultSessionsHandler::method() { return HttpMethod::GET; }

void VaultSessionsHandler::setVaultPath(const std::string& path) { vaultPath_ = path; }

static std::string toUtf8(const fs::path& p)
{
    auto u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

static std::string jsonEscape(const std::string& s)
{
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
void VaultSessionsHandler::handle(const char*, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);
    res->set_content("{\"sessions\":[]}", "application/json");
}

void VaultSessionsHandler::handleWithContext(
    const char* /*body*/, const char* /*path*/, const char* query, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);

    if (vaultPath_.empty()) {
        res->set_content("{\"error\":\"Vault path not set\"}", "application/json");
        res->status = 500;
        return;
    }

    // Check if requesting a specific session
    std::string sessionName;
    if (query && std::strncmp(query, "session=", 8) == 0) {
        sessionName = query + 8;
    }

    // Build dialogs path using string concat (avoid mixed slashes from fs::path::operator/)
    std::string dialogsPathStr = vaultPath_;
    if (!dialogsPathStr.empty() && dialogsPathStr.back() != '/')
        dialogsPathStr += '/';
    dialogsPathStr += "Диалоги";
    fs::path dialogsPath = fs::u8path(dialogsPathStr);

    if (sessionName.empty()) {
        // --- List sessions ---
        std::ostringstream json;
        json << "{\"sessions\":[";
        bool first = true;

        if (fs::exists(dialogsPath) && fs::is_directory(dialogsPath)) {
            std::vector<fs::directory_entry> entries;
            for (const auto& e : fs::directory_iterator(dialogsPath))
                entries.push_back(e);
            std::sort(entries.begin(), entries.end(),
                [](const auto& a, const auto& b) {
                    return a.path().filename().string() > b.path().filename().string();
                });

            for (const auto& entry : entries) {
                if (!entry.is_regular_file()) continue;
                std::string filename = toUtf8(entry.path().filename());
                if (filename.size() < 4 || filename.substr(filename.size() - 3) != ".md")
                    continue;

                std::string stem = filename.substr(0, filename.size() - 3);
                std::string title = stem;

                // Count messages: lines starting with "## " or "### " as separator,
                // count alternating user/assistant blocks.
                int msgCount = 0;
                std::string dateStr = "";
                {
                    std::ifstream f(entry.path());
                    if (f) {
                        std::string line;
                        while (std::getline(f, line)) {
                            // Try to extract date from first heading
                            if (dateStr.empty()) {
                                std::regex dateRe(R"(## (\d{4}-\d{2}-\d{2}))");
                                std::smatch m;
                                if (std::regex_search(line, m, dateRe))
                                    dateStr = m[1].str();
                            }
                            // Count role markers
                            if (line.find("**User:**") != std::string::npos ||
                                line.find("**Assistant:**") != std::string::npos ||
                                line.find("**Мишка:**") != std::string::npos ||
                                line.find("**Катя:**") != std::string::npos) {
                                ++msgCount;
                            }
                        }
                    }
                }
                if (dateStr.empty()) dateStr = stem.substr(0, 10);  // fallback

                if (!first) json << ",";
                first = false;
                json << "{\"id\":\"" << stem
                     << "\",\"title\":\"" << jsonEscape(title)
                     << "\",\"date\":\"" << dateStr
                     << "\",\"messageCount\":" << msgCount
                     << ",\"path\":\"Диалоги/" << filename << "\"}";
            }
        }

        json << "]}";
        res->set_content(json.str(), "application/json; charset=utf-8");

    } else {
        // --- Load single session ---
        // Build session file path using string concat
        std::string sessionPathStr = dialogsPathStr + "/" + sessionName + ".md";
        fs::path sessionPath = fs::u8path(sessionPathStr);
        if (!fs::exists(sessionPath)) {
            res->set_content("{\"error\":\"Session not found\"}", "application/json");
            res->status = 404;
            return;
        }

        std::ifstream f(sessionPath);
        if (!f) {
            res->set_content("{\"error\":\"Cannot read session\"}", "application/json");
            res->status = 500;
            return;
        }

        std::ostringstream json;
        json << "{\"messages\":[";
        bool first = true;

        std::string line;
        std::string currentRole;
        std::string currentContent;
        int msgId = 0;

        // Simple parser: **Role:** starts a message block
        std::regex roleRe(R"(\*\*([^*]+):\*\*)");
        while (std::getline(f, line)) {
            std::smatch m;
            if (std::regex_search(line, m, roleRe)) {
                // Flush previous message
                if (!currentContent.empty() && !currentRole.empty()) {
                    if (!first) json << ",";
                    first = false;
                    json << "{\"id\":\"msg-" << msgId++
                         << "\",\"role\":\"" << currentRole
                         << "\",\"content\":\"" << jsonEscape(currentContent) << "\"}";
                }
                currentRole = m[1].str();
                currentContent = line.substr(m.position() + m.length());
                // Trim
                while (!currentContent.empty() && currentContent.front() == ' ')
                    currentContent.erase(0, 1);
            } else if (!line.empty() && !currentRole.empty()) {
                if (!currentContent.empty()) currentContent += "\\n";
                currentContent += line;
            }
        }
        // Flush last message
        if (!currentContent.empty() && !currentRole.empty()) {
            if (!first) json << ",";
            json << "{\"id\":\"msg-" << msgId++
                 << "\",\"role\":\"" << currentRole
                 << "\",\"content\":\"" << jsonEscape(currentContent) << "\"}";
        }

        json << "]}";
        res->set_content(json.str(), "application/json; charset=utf-8");
    }
}

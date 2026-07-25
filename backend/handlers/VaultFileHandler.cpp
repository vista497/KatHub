#include "VaultFileHandler.h"

#include "httplib.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
const char* VaultFileHandler::route() { return "/api/vault/file"; }

IHttpHandler::HttpMethod VaultFileHandler::method() { return HttpMethod::GET; }

void VaultFileHandler::setVaultPath(const std::string& path) { vaultPath_ = path; }

// ---------------------------------------------------------------------------
// Base handle() — this handler needs query params, so delegate to
// handleWithContext with empty body/path/query for a reasonable fallback.
void VaultFileHandler::handle(const char* /*request*/, void* response)
{
    handleWithContext(nullptr, nullptr, nullptr, response);
}

// Helper: convert fs::path to UTF-8
static std::string toUtf8(const fs::path& p)
{
    auto u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

// ---------------------------------------------------------------------------
void VaultFileHandler::handleWithContext(
    const char* /*body*/, const char* /*path*/, const char* query, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);

    // URL-decode helper
    auto urlDecode = [](const std::string& s) -> std::string {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '%' && i + 2 < s.size()) {
                int hi = s[i+1] >= 'A' ? (s[i+1] & 0xDF) - 'A' + 10 : s[i+1] - '0';
                int lo = s[i+2] >= 'A' ? (s[i+2] & 0xDF) - 'A' + 10 : s[i+2] - '0';
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
            } else if (s[i] == '+') {
                out += ' ';
            } else {
                out += s[i];
            }
        }
        return out;
    };

    // Parse path= param from query string
    std::string fileRelPath;
    if (query && std::strncmp(query, "path=", 5) == 0) {
        fileRelPath = urlDecode(query + 5);
    }

    if (fileRelPath.empty() || vaultPath_.empty()) {
        res->set_content("{\"error\":\"Missing path parameter\"}", "application/json");
        res->status = 400;
        return;
    }

    // Sanity check: don't allow path traversal
    if (fileRelPath.find("..") != std::string::npos) {
        res->set_content("{\"error\":\"Invalid path\"}", "application/json");
        res->status = 403;
        return;
    }

    // Build full path using string concatenation to avoid mixed separators
    // (fs::path::operator/ uses platform separator, but vaultPath_ uses forward slashes)
    std::string fullPathStr = vaultPath_;
    if (!fullPathStr.empty() && fullPathStr.back() != '/')
        fullPathStr += '/';
    fullPathStr += fileRelPath;
    fs::path fullPath = fs::u8path(fullPathStr);

    if (!fs::exists(fullPath) || !fs::is_regular_file(fullPath)) {
        res->set_content("{\"error\":\"File not found\"}", "application/json");
        res->status = 404;
        return;
    }

    // Read file
    std::ifstream f(fullPath);
    if (!f) {
        res->set_content("{\"error\":\"Cannot read file\"}", "application/json");
        res->status = 500;
        return;
    }

    std::ostringstream buf;
    buf << f.rdbuf();
    std::string content = buf.str();

    // Escape for JSON
    auto jsonEscape = [](const std::string& s) -> std::string {
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
    };

    std::ostringstream json;
    json << "{\"path\":\"" << fileRelPath
         << "\",\"content\":\"" << jsonEscape(content) << "\"}";
    res->set_content(json.str(), "application/json; charset=utf-8");
}

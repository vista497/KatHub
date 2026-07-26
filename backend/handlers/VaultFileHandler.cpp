#include "VaultFileHandler.h"

#include "httplib.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cctype>
#include <iomanip>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
const char* VaultFileHandler::route() { return "/api/vault/file"; }

IHttpHandler::HttpMethod VaultFileHandler::method() { return HttpMethod::GET; }

void VaultFileHandler::setVaultPath(const std::string& path) { vaultPath_ = path; }

// ---------------------------------------------------------------------------
void VaultFileHandler::handle(const char* /*request*/, void* response)
{
    handleWithContext(nullptr, nullptr, nullptr, response);
}

// ---------------------------------------------------------------------------
// Proper JSON string escape — handles ALL control characters
static std::string jsonEscape(const std::string& s)
{
    std::ostringstream out;
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b";  break;
            case '\f': out << "\\f";  break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;
            default:
                if (c < 0x20) {
                    // Control char → \u00XX
                    out << "\\u00" << std::hex << std::uppercase
                        << std::setw(2) << std::setfill('0') << static_cast<int>(c)
                        << std::dec << std::nouppercase;
                } else {
                    out << c;
                }
                break;
        }
    }
    return out.str();
}

// ---------------------------------------------------------------------------
void VaultFileHandler::handleWithContext(
    const char* /*body*/, const char* /*path*/, const char* query, void* response,
    const char* /*method*/)
{
    auto* res = static_cast<httplib::Response*>(response);

    // URL-decode helper (proper hex decoding)
    auto urlDecode = [](const std::string& s) -> std::string {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '%' && i + 2 < s.size()
                && std::isxdigit(static_cast<unsigned char>(s[i+1]))
                && std::isxdigit(static_cast<unsigned char>(s[i+2]))) {
                unsigned int hi = std::isdigit(static_cast<unsigned char>(s[i+1]))
                    ? s[i+1] - '0'
                    : (std::toupper(s[i+1]) - 'A' + 10);
                unsigned int lo = std::isdigit(static_cast<unsigned char>(s[i+2]))
                    ? s[i+2] - '0'
                    : (std::toupper(s[i+2]) - 'A' + 10);
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
        res->set_content(R"({"error":"Missing path parameter"})", "application/json");
        res->status = 400;
        return;
    }

    // Sanity check: no path traversal
    if (fileRelPath.find("..") != std::string::npos) {
        res->set_content(R"({"error":"Invalid path"})", "application/json");
        res->status = 403;
        return;
    }

    // Normalize: replace backslashes with forward slashes (Windows paths)
    for (auto& ch : fileRelPath) {
        if (ch == '\\') ch = '/';
    }

    // Build full path
    std::string fullPathStr = vaultPath_;
    if (!fullPathStr.empty() && fullPathStr.back() != '/' && fullPathStr.back() != '\\')
        fullPathStr += '/';
    fullPathStr += fileRelPath;
    fs::path fullPath = fs::u8path(fullPathStr);

    if (!fs::exists(fullPath) || !fs::is_regular_file(fullPath)) {
        res->set_content(R"({"error":"File not found"})", "application/json");
        res->status = 404;
        return;
    }

    // Read file as binary to catch all bytes
    std::ifstream f(fullPath, std::ios::binary);
    if (!f) {
        res->set_content(R"({"error":"Cannot read file"})", "application/json");
        res->status = 500;
        return;
    }

    std::ostringstream buf;
    buf << f.rdbuf();
    std::string content = buf.str();

    // Build JSON with proper escaping
    std::ostringstream json;
    json << "{\"path\":\"" << jsonEscape(fileRelPath)
         << "\",\"exists\":true,\"content\":\"" << jsonEscape(content) << "\"}";
    res->set_content(json.str(), "application/json; charset=utf-8");
}

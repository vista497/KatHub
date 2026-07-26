#include "VaultGraphHandler.h"
#include "PluginRegistry.h"

#include "httplib.h"

#ifdef DELETE
#undef DELETE
#endif

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

// Helper: convert fs::path to UTF-8 std::string with forward slashes
static std::string toUtf8(const fs::path& p)
{
    auto u8 = p.u8string();
    std::string s(reinterpret_cast<const char*>(u8.data()), u8.size());
    // Normalize backslashes to forward slashes
    for (auto& ch : s) {
        if (ch == '\\') ch = '/';
    }
    return s;
}

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

// Auto-registration
REGISTER_HANDLER(VaultGraphHandler)

// ---------------------------------------------------------------------------
VaultGraphHandler::VaultGraphHandler() = default;

const char* VaultGraphHandler::route()
{
    return "/api/vault/graph";
}

IHttpHandler::HttpMethod VaultGraphHandler::method()
{
    return IHttpHandler::HttpMethod::GET;
}

void VaultGraphHandler::setVaultPath(const std::string& path)
{
    vaultPath_ = path;
}

// ---------------------------------------------------------------------------
void VaultGraphHandler::handle(const char* /*request*/, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);

    std::ostringstream json;

    if (vaultPath_.empty() || !fs::exists(vaultPath_)) {
        std::string dbg = "empty=" + std::string(vaultPath_.empty() ? "1" : "0")
            + " exists=" + std::string(fs::exists(vaultPath_) ? "1" : "0");
        json << "{\"nodes\":[],\"links\":[],\"debug\":\"" << dbg << "\"}";
        res->set_content(json.str(), "application/json");
        return;
    }

    buildGraphJson(json);
    res->set_content(json.str(), "application/json; charset=utf-8");
}

// ---------------------------------------------------------------------------
std::string VaultGraphHandler::readFile(const std::string& path)
{
    std::ifstream f(fs::path(fs::u8path(path)), std::ios::binary);
    if (!f) return "";
    std::stringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

bool VaultGraphHandler::isMarkdown(const std::string& path)
{
    return path.size() >= 3
        && path.compare(path.size() - 3, 3, ".md") == 0;
}

// ---------------------------------------------------------------------------
void VaultGraphHandler::buildGraphJson(std::ostringstream& json)
{
    json.str("");
    json.clear();

    std::ostringstream nodesJson;
    std::ostringstream linksJson;
    nodesJson << "[";
    linksJson << "[";

    bool firstNode = true;
    bool firstLink = true;
    int nodeCount = 0;
    int linkCount = 0;

    auto addNode = [&](const std::string& id,
                       const std::string& label,
                       const std::string& type,
                       const std::string& folder)
    {
        if (!firstNode) nodesJson << ",";
        firstNode = false;
        ++nodeCount;
        nodesJson << "{\"id\":\"" << jsonEscape(id)
                  << "\",\"label\":\"" << jsonEscape(label)
                  << "\",\"type\":\"" << jsonEscape(type)
                  << "\",\"folder\":\"" << jsonEscape(folder) << "\"}";
    };

    auto addLink = [&](const std::string& source,
                       const std::string& target,
                       const std::string& type)
    {
        if (!firstLink) linksJson << ",";
        firstLink = false;
        ++linkCount;
        linksJson << "{\"source\":\"" << jsonEscape(source)
                  << "\",\"target\":\"" << jsonEscape(target)
                  << "\",\"type\":\"" << jsonEscape(type) << "\"}";
    };

    // 1. Folders as nodes
    for (const auto& entry : fs::directory_iterator(fs::u8path(vaultPath_))) {
        if (!entry.is_directory()) continue;
        std::string folder = toUtf8(entry.path().filename());
        if (!folder.empty() && folder[0] == '.') continue;

        addNode(folder, folder, "folder", folder);
    }

    // 2. Collect all .md files for path resolution
    std::map<std::string, std::string> titleToPath;
    for (const auto& entry : fs::recursive_directory_iterator(fs::u8path(vaultPath_))) {
        if (!entry.is_regular_file()) continue;
        std::string relPath = toUtf8(fs::relative(entry.path(), fs::u8path(vaultPath_)));
        if (!isMarkdown(relPath)) continue;
        std::string stem = toUtf8(fs::path(fs::u8path(relPath)).stem());
        if (titleToPath.find(stem) == titleToPath.end()
            || relPath.size() < titleToPath[stem].size()) {
            titleToPath[stem] = relPath;
        }
    }

    // 3. Read .md files, add note nodes + wikilinks
    std::regex wikilinkRe(R"(\[\[([^\]]+)\]\])");

    for (const auto& entry : fs::recursive_directory_iterator(fs::u8path(vaultPath_))) {
        if (!entry.is_regular_file()) continue;
        std::string relPath = toUtf8(fs::relative(entry.path(), fs::u8path(vaultPath_)));
        if (!isMarkdown(relPath)) continue;

        fs::path relPathFs = fs::u8path(relPath);
        std::string folder = toUtf8(relPathFs.parent_path());
        if (folder == "." || folder.empty()) folder = "root";

        std::string title = toUtf8(relPathFs.stem());
        std::string content = readFile(toUtf8(entry.path()));

        // Use relPath as id (already forward-slash normalized)
        addNode(relPath, title, "note", folder);

        // Find [[wikilinks]]
        auto begin = std::sregex_iterator(content.begin(), content.end(), wikilinkRe);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::string targetRaw = (*it)[1].str();

            size_t pipe = targetRaw.find('|');
            std::string target = (pipe != std::string::npos)
                ? targetRaw.substr(0, pipe) : targetRaw;

            while (!target.empty() && target.front() == ' ') target.erase(0, 1);
            while (!target.empty() && target.back() == ' ') target.pop_back();

            if (target.empty()) continue;

            auto it2 = titleToPath.find(target);
            if (it2 != titleToPath.end()) {
                addLink(relPath, it2->second, "wikilink");
            }
        }
    }

    nodesJson << "]";
    linksJson << "]";

    json << "{\"nodes\":" << nodesJson.str()
         << ",\"links\":" << linksJson.str()
         << ",\"debug\":\"n=" << nodeCount << " l=" << linkCount << "\"}";
}

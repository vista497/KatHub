#include "VaultGraphHandler.h"
#include "PluginRegistry.h"

#include "httplib.h"

#ifdef DELETE
#undef DELETE
#endif

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

// Helper: convert fs::path to UTF-8 std::string
static std::string toUtf8(const fs::path& p)
{
    auto u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
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
    res->set_content(json.str(), "application/json");
}

// ---------------------------------------------------------------------------
std::string VaultGraphHandler::readFile(const std::string& path)
{
    // Use wide-string path for proper Unicode support on Windows
    std::ifstream f(fs::path(fs::u8path(path)));
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
    json.str("");  // clear
    json.clear();

    std::ostringstream nodesJson;
    std::ostringstream linksJson;
    nodesJson << "[";
    linksJson << "[";

    bool firstNode = true;
    bool firstLink = true;
    int nodeCount = 0;
    int linkCount = 0;

    // Escape backslashes and quotes for JSON
    auto esc = [](const std::string& s) -> std::string {
        std::string out;
        for (char c : s) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else out += c;
        }
        return out;
    };

    // Helper to add a node (auto-escapes all fields)
    auto addNode = [&](const std::string& id,
                       const std::string& label,
                       const std::string& type,
                       const std::string& folder)
    {
        if (!firstNode) nodesJson << ",";
        firstNode = false;
        ++nodeCount;
        nodesJson << "{\"id\":\"" << esc(id)
                  << "\",\"label\":\"" << esc(label)
                  << "\",\"type\":\"" << esc(type)
                  << "\",\"folder\":\"" << esc(folder) << "\"}";
    };

    // Helper to add a link (auto-escapes)
    auto addLink = [&](const std::string& source,
                       const std::string& target,
                       const std::string& type)
    {
        if (!firstLink) linksJson << ",";
        firstLink = false;
        ++linkCount;
        linksJson << "{\"source\":\"" << esc(source)
                  << "\",\"target\":\"" << esc(target)
                  << "\",\"type\":\"" << esc(type) << "\"}";
    };

    // 1. Folders as nodes
    for (const auto& entry : fs::directory_iterator(fs::u8path(vaultPath_))) {
        if (!entry.is_directory()) continue;
        std::string folder = toUtf8(entry.path().filename());
        if (!folder.empty() && folder[0] == '.') continue;  // skip hidden

        addNode(folder, folder, "folder", folder);
    }

    // 2. Collect all .md files for path resolution
    std::map<std::string, std::string> titleToPath;  // stem → relative path
    for (const auto& entry : fs::recursive_directory_iterator(fs::u8path(vaultPath_))) {
        if (!entry.is_regular_file()) continue;
        std::string relPath = toUtf8(fs::relative(entry.path(), fs::u8path(vaultPath_)));
        if (!isMarkdown(relPath)) continue;
        std::string stem = toUtf8(fs::path(fs::u8path(relPath)).stem());
        // Use the first match (shorter path wins for disambiguation)
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
        std::string content = readFile(
            toUtf8(entry.path()));

        addNode(relPath, esc(title), "note", folder);

        // Find [[wikilinks]]
        auto begin = std::sregex_iterator(content.begin(), content.end(), wikilinkRe);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::string targetRaw = (*it)[1].str();

            // Strip alias: [[page|alias]] → page
            size_t pipe = targetRaw.find('|');
            std::string target = (pipe != std::string::npos)
                ? targetRaw.substr(0, pipe) : targetRaw;

            // Trim whitespace
            while (!target.empty() && target.front() == ' ') target.erase(0, 1);
            while (!target.empty() && target.back() == ' ') target.pop_back();

            if (target.empty()) continue;

            // Look up in title→path map
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

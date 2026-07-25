#pragma once

#include <string>

// Forward declarations
namespace httplib {
    class Server;
}

// Serves static files from a directory using httplib's set_mount_point.
// Can optionally set an index file fallback.
class StaticFileHandler
{
public:
    StaticFileHandler() = default;

    // Mount a directory for static file serving.
    // E.g., serveDir("./public") makes files in ./public available at /.
    void serveDir(httplib::Server &server, const std::string &path, const std::string &urlPrefix = "/");

    // Set the fallback index file name (e.g., "index.html").
    // Files accessed as /dir/ will serve /dir/index.html if it exists.
    void setIndexFile(const std::string &indexFile);

    const std::string &indexFile() const;

private:
    std::string indexFile_{"index.html"};
};

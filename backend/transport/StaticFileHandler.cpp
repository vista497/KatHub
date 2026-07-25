#include "StaticFileHandler.h"

#include "httplib.h"

void StaticFileHandler::serveDir(httplib::Server &server,
                                  const std::string &path,
                                  const std::string &urlPrefix)
{
    // httplib::Server::set_mount_point serves static files efficiently
    server.set_mount_point(urlPrefix, path);
}

void StaticFileHandler::setIndexFile(const std::string &indexFile)
{
    indexFile_ = indexFile;
}

const std::string &StaticFileHandler::indexFile() const
{
    return indexFile_;
}

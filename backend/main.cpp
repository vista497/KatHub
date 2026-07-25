#include "KatHubApp.h"

#include <cstring>
#include <iostream>

static void printUsage(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS]\n"
        << "\n"
        << "Options:\n"
        << "  --server           Run in HTTP server mode (default)\n"
        << "  --hand             Run in Hand mode (WebEngine GUI)\n"
        << "  --port <port>      HTTP server port (default: 8080)\n"
        << "  --ws-port <port>   WebSocket server port (default: 8081)\n"
        << "  --host <hostname>  Host for Hand mode WebEngine to connect to\n"
        << "                     (default: localhost)\n"
        << "  --config <path>    JSON configuration file path\n"
        << std::endl;
}

int main(int argc, char *argv[])
{
    // Parse mode from args; defaults to Server.
    KatHubApp::Mode mode = KatHubApp::Mode::Server;

    // Simple pre-parse for mode (before QCoreApplication exists).
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--hand") == 0) {
            mode = KatHubApp::Mode::Hand;
            break;
        }
        if (std::strcmp(argv[i], "--server") == 0) {
            mode = KatHubApp::Mode::Server;
            break;
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        }
    }

    KatHubApp app(argc, argv, mode);
    app.configureServices();
    app.init();
    return app.run();
}

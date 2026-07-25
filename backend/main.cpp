#include "KatHubApp.h"

#include <cstring>

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
    }

    KatHubApp app(argc, argv, mode);
    app.init();
    return app.run();
}

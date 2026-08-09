#include "editor/EditorApp.h"
#include "core/Logger.h"
#include "core/Version.h"
#include <iostream>
#include <exception>

int main() {
    try {
        LOG_INFO("Starting Khepri Engine v" + std::string(KhepriEngine::VERSION_STRING) + "...");
        EditorApp app;
        app.Run();
        LOG_INFO("Khepri Engine shut down gracefully.");
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#include "editor/EditorApp.h"
#include "core/Logger.h"
#include <iostream>
#include <exception>

int main() {
    try {
        LOG_INFO("Starting Vulkan Graphics & Computational Geometry Engine...");
        EditorApp app;
        app.Run();
        LOG_INFO("Vulkan Geometry Engine shut down gracefully.");
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

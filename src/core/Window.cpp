#include "Window.h"
#include "Logger.h"
#include <stdexcept>

Window::Window(int width, int height, const std::string& title)
    : m_width(width), m_height(height), m_title(title) {
    if (!glfwInit()) {
        LOG_ERROR("Failed to initialize GLFW");
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Don't create OpenGL context for Vulkan
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!m_window) {
        LOG_ERROR("Failed to create GLFW window");
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);

    LOG_INFO("GLFW Window created successfully (" + std::to_string(width) + "x" + std::to_string(height) + ")");
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
    LOG_INFO("Window destroyed and GLFW terminated");
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::PollEvents() {
    glfwPollEvents();
}

void Window::WaitEvents() {
    glfwWaitEvents();
}

void Window::WaitEventsTimeout(double timeoutSeconds) {
    glfwWaitEventsTimeout(timeoutSeconds);
}

void Window::GetFramebufferSize(int* width, int* height) const {
    if (m_window) {
        glfwGetFramebufferSize(m_window, width, height);
    } else {
        if (width) *width = m_width;
        if (height) *height = m_height;
    }
}

bool Window::IsMinimized() const noexcept {
    int fbW = 0, fbH = 0;
    GetFramebufferSize(&fbW, &fbH);
    return fbW == 0 || fbH == 0;
}

void Window::ToggleFullscreen() {
    SetFullscreen(!m_isFullscreen);
}

void Window::SetFullscreen(bool fullscreen) {
    if (m_isFullscreen == fullscreen || !m_window) return;

    if (fullscreen) {
        // Save current windowed geometry
        glfwGetWindowPos(m_window, &m_savedWindowedX, &m_savedWindowedY);
        glfwGetWindowSize(m_window, &m_savedWindowedWidth, &m_savedWindowedHeight);

        // Find the monitor where the window currently resides
        int bestX = m_savedWindowedX + m_savedWindowedWidth / 2;
        int bestY = m_savedWindowedY + m_savedWindowedHeight / 2;

        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        GLFWmonitor* targetMonitor = glfwGetPrimaryMonitor();

        for (int i = 0; i < monitorCount; ++i) {
            int mx = 0, my = 0;
            glfwGetMonitorPos(monitors[i], &mx, &my);
            const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
            if (mode) {
                if (bestX >= mx && bestX < mx + mode->width &&
                    bestY >= my && bestY < my + mode->height) {
                    targetMonitor = monitors[i];
                    break;
                }
            }
        }

        const GLFWvidmode* mode = glfwGetVideoMode(targetMonitor);
        if (mode) {
            m_isFullscreen = true;
            glfwSetWindowMonitor(m_window, targetMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            LOG_INFO("Switched to Fullscreen mode (" + std::to_string(mode->width) + "x" + std::to_string(mode->height) + ")");
        }
    } else {
        m_isFullscreen = false;
        glfwSetWindowMonitor(m_window, nullptr, m_savedWindowedX, m_savedWindowedY, m_savedWindowedWidth, m_savedWindowedHeight, 0);
        LOG_INFO("Restored to Windowed mode (" + std::to_string(m_savedWindowedWidth) + "x" + std::to_string(m_savedWindowedHeight) + ")");
    }
}

void Window::SetTitle(const std::string& title) {
    m_title = title;
    if (m_window) {
        glfwSetWindowTitle(m_window, m_title.c_str());
    }
}

VkSurfaceKHR Window::CreateSurface(VkInstance instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, m_window, nullptr, &surface) != VK_SUCCESS) {
        LOG_ERROR("Failed to create window surface");
        throw std::runtime_error("Failed to create window surface");
    }
    return surface;
}

void Window::FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto win = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win) {
        win->m_width = width;
        win->m_height = height;
        win->m_resized = true;
        if (win->m_resizeCallback) {
            win->m_resizeCallback(width, height);
        }
    }
}

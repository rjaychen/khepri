#pragma once

#include <volk.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <string>
#include <functional>

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ShouldClose() const;
    void PollEvents();
    
    GLFWwindow* GetNativeWindow() const { return m_window; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    bool WasResized() const { return m_resized; }
    void ResetResizedFlag() { m_resized = false; }

    VkSurfaceKHR CreateSurface(VkInstance instance);

    void SetTitle(const std::string& title);
    const std::string& GetTitle() const { return m_title; }

    void SetResizeCallback(std::function<void(int, int)> callback) {
        m_resizeCallback = callback;
    }

private:
    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* m_window = nullptr;
    int m_width;
    int m_height;
    std::string m_title;
    bool m_resized = false;
    std::function<void(int, int)> m_resizeCallback;
};

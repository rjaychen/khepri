#pragma once

#include <volk.h>
#include <vector>
#include <memory>
#include "VulkanContext.h"
#include "Buffer.h"
#include "VulkanSync.h"

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Swapchain {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    /// Controls the Vulkan swapchain present mode.
    /// Mailbox  — triple-buffered, lowest latency, no tearing (default).
    /// Immediate — uncapped throughput, possible tearing, lowest latency.
    /// VSync    — FIFO, capped at monitor refresh rate, no tearing, adds up to 1 frame latency.
    enum class PresentMode { Mailbox, Immediate, VSync };

    Swapchain(VulkanContext& context, uint32_t width, uint32_t height);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void Recreate(uint32_t width, uint32_t height);

    /// Request a present mode change. Takes effect on the next Recreate() call,
    /// which is triggered automatically if m_pendingPresentModeChange is set.
    void SetPresentMode(PresentMode mode);
    [[nodiscard]] PresentMode GetPresentMode() const noexcept { return m_presentMode; }
    /// Returns true if a present mode change is pending and the swapchain needs recreation.
    [[nodiscard]] bool HasPendingPresentModeChange() const noexcept { return m_pendingPresentModeChange; }
    /// Apply pending present mode change. Call from the main loop at a safe point (after fence wait).
    void ApplyPendingPresentModeChange();

    VkSwapchainKHR GetSwapchain() const { return m_swapchain; }
    VkFormat GetImageFormat() const { return m_imageFormat; }
    VkFormat GetDepthFormat() const { return m_depthFormat; }
    VkExtent2D GetExtent() const { return m_extent; }
    const std::vector<VkImage>& GetImages() const { return m_images; }
    const std::vector<VkImageView>& GetImageViews() const { return m_imageViews; }
    uint32_t GetImageCount() const { return static_cast<uint32_t>(m_images.size()); }
    VkImageView GetDepthImageView() const { return m_depthImageView; }

    VkResult AcquireNextImage(uint32_t* imageIndex);
    VkResult Present(uint32_t imageIndex);

    // imageAvailableSemaphores[imageIndex] is the semaphore to wait on before rendering to that image.
    // Always read this AFTER AcquireNextImage() returns the imageIndex.
    VkSemaphore GetImageAvailableSemaphore(uint32_t imageIndex) const { return m_imageAvailableSemaphores[imageIndex].GetHandle(); }
    VkSemaphore GetRenderFinishedSemaphore(uint32_t imageIndex) const { return m_renderFinishedSemaphores[imageIndex].GetHandle(); }
    VkFence GetInFlightFence(uint32_t frame) const { return m_inFlightFences[frame].GetHandle(); }

    static SwapchainSupportDetails QuerySupport(VkPhysicalDevice device, VkSurfaceKHR surface);

private:
    void CreateSwapchain(uint32_t width, uint32_t height);
    void CreateImageViews();
    void CreateDepthResources();
    void CreateSyncObjects();
    void Cleanup();

    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
    VkFormat FindDepthFormat();

    VulkanContext& m_context;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_imageFormat;
    VkFormat m_depthFormat;
    VkExtent2D m_extent;

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

    VkImage m_depthImage = VK_NULL_HANDLE;
    VmaAllocation m_depthImageAllocation = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    // Per-swapchain-image semaphores (indexed by imageIndex returned from vkAcquireNextImageKHR).
    // A spare semaphore is used during acquire and then swapped in so each image always
    // owns its semaphore and cannot be reused while the display engine still holds it.
    std::vector<khepri::VulkanSemaphore> m_imageAvailableSemaphores; // size = image count
    khepri::VulkanSemaphore m_spareSemaphore;       // rotating acquire target
    std::vector<khepri::VulkanSemaphore> m_renderFinishedSemaphores;
    std::vector<khepri::VulkanFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;

    PresentMode m_presentMode = PresentMode::Mailbox;
    bool m_pendingPresentModeChange = false;
};

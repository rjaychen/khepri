#pragma once

#include <volk.h>
#include <vector>
#include <memory>
#include "VulkanContext.h"
#include "Buffer.h"

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Swapchain {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    Swapchain(VulkanContext& context, uint32_t width, uint32_t height);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void Recreate(uint32_t width, uint32_t height);

    VkSwapchainKHR GetSwapchain() const { return m_swapchain; }
    VkFormat GetImageFormat() const { return m_imageFormat; }
    VkFormat GetDepthFormat() const { return m_depthFormat; }
    VkExtent2D GetExtent() const { return m_extent; }
    const std::vector<VkImageView>& GetImageViews() const { return m_imageViews; }
    VkImageView GetDepthImageView() const { return m_depthImageView; }

    VkResult AcquireNextImage(uint32_t* imageIndex);
    VkResult Present(uint32_t imageIndex);

    VkSemaphore GetImageAvailableSemaphore(uint32_t frame) const { return m_imageAvailableSemaphores[frame]; }
    VkSemaphore GetRenderFinishedSemaphore(uint32_t frame) const { return m_renderFinishedSemaphores[frame]; }
    VkFence GetInFlightFence(uint32_t frame) const { return m_inFlightFences[frame]; }

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

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
};

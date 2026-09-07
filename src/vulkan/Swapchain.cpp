#include "Swapchain.h"
#include "../core/Logger.h"
#include <algorithm>
#include <stdexcept>

Swapchain::Swapchain(VulkanContext& context, uint32_t width, uint32_t height)
    : m_context(context) {
    CreateSwapchain(width, height);
    CreateImageViews();
    CreateDepthResources();
    CreateSyncObjects();

    LOG_INFO("Swapchain initialized (" + std::to_string(m_extent.width) + "x" + std::to_string(m_extent.height) + ")");
}

Swapchain::~Swapchain() {
    Cleanup();
    if (m_spareSemaphore) vkDestroySemaphore(m_context.GetDevice(), m_spareSemaphore, nullptr);
    for (size_t i = 0; i < m_imageAvailableSemaphores.size(); i++) {
        vkDestroySemaphore(m_context.GetDevice(), m_imageAvailableSemaphores[i], nullptr);
    }
    for (size_t i = 0; i < m_renderFinishedSemaphores.size(); i++) {
        vkDestroySemaphore(m_context.GetDevice(), m_renderFinishedSemaphores[i], nullptr);
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyFence(m_context.GetDevice(), m_inFlightFences[i], nullptr);
    }
}

void Swapchain::Cleanup() {
    if (m_depthImageView) {
        vkDestroyImageView(m_context.GetDevice(), m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }
    if (m_depthImage) {
        vmaDestroyImage(m_context.GetAllocator(), m_depthImage, m_depthImageAllocation);
        m_depthImage = VK_NULL_HANDLE;
    }
    for (auto imageView : m_imageViews) {
        vkDestroyImageView(m_context.GetDevice(), imageView, nullptr);
    }
    m_imageViews.clear();

    if (m_swapchain) {
        vkDestroySwapchainKHR(m_context.GetDevice(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

void Swapchain::Recreate(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    m_context.WaitIdle();
    Cleanup();
    CreateSwapchain(width, height);
    CreateImageViews();
    CreateDepthResources();
    LOG_INFO("Swapchain recreated (" + std::to_string(m_extent.width) + "x" + std::to_string(m_extent.height) + ")");
}

SwapchainSupportDetails Swapchain::QuerySupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void Swapchain::CreateSwapchain(uint32_t width, uint32_t height) {
    SwapchainSupportDetails swapChainSupport = QuerySupport(m_context.GetPhysicalDevice(), m_context.GetSurface());

    VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChoosePresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = ChooseExtent(swapChainSupport.capabilities, width, height);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_context.GetSurface();

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    QueueFamilyIndices indices = m_context.GetQueueFamilies();
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_context.GetDevice(), &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) {
        LOG_ERROR("Failed to create swap chain!");
        throw std::runtime_error("Failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(m_context.GetDevice(), m_swapchain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_context.GetDevice(), m_swapchain, &imageCount, m_images.data());

    m_imageFormat = surfaceFormat.format;
    m_extent = extent;
}

void Swapchain::CreateImageViews() {
    m_imageViews.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_imageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_context.GetDevice(), &createInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create image views!");
            throw std::runtime_error("Failed to create image views!");
        }
    }
}

void Swapchain::CreateDepthResources() {
    m_depthFormat = FindDepthFormat();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_extent.width;
    imageInfo.extent.height = m_extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    if (vmaCreateImage(m_context.GetAllocator(), &imageInfo, &allocInfo, &m_depthImage, &m_depthImageAllocation, nullptr) != VK_SUCCESS) {
        LOG_ERROR("Failed to create depth image!");
        throw std::runtime_error("Failed to create depth image!");
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_context.GetDevice(), &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS) {
        LOG_ERROR("Failed to create depth image view!");
        throw std::runtime_error("Failed to create depth image view!");
    }
}

void Swapchain::CreateSyncObjects() {
    // imageAvailableSemaphores & renderFinishedSemaphores: one per swapchain image.
    // Indexing both by imageIndex guarantees semaphores are never reused while presentation of that image is pending.
    m_imageAvailableSemaphores.resize(m_images.size());
    m_renderFinishedSemaphores.resize(m_images.size());
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_spareSemaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create spare semaphore!");
    }
    for (size_t i = 0; i < m_images.size(); i++) {
        if (vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_context.GetDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create per-image semaphores!");
        }
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateFence(m_context.GetDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create fence!");
        }
    }
}

VkResult Swapchain::AcquireNextImage(uint32_t* imageIndex) {
    vkWaitForFences(m_context.GetDevice(), 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(m_context.GetDevice(), 1, &m_inFlightFences[m_currentFrame]);

    // Signal m_spareSemaphore (always unsignaled at this point).
    VkResult result = vkAcquireNextImageKHR(
        m_context.GetDevice(), m_swapchain, UINT64_MAX,
        m_spareSemaphore, VK_NULL_HANDLE, imageIndex
    );

    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
        // Swap spare with the per-image semaphore so:
        //   imageAvailableSemaphores[imageIndex] = freshly signaled semaphore (submit waits on this)
        //   m_spareSemaphore = old per-image semaphore (already consumed, safe to reuse next acquire)
        std::swap(m_spareSemaphore, m_imageAvailableSemaphores[*imageIndex]);
    }

    return result;
}

VkResult Swapchain::Present(uint32_t imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[imageIndex] };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { m_swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = m_context.GetPresentQueue().Present(presentInfo);

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return result;
}

VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR Swapchain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; // Guaranteed to be supported
}

VkExtent2D Swapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = { width, height };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

VkFormat Swapchain::FindDepthFormat() {
    std::vector<VkFormat> candidates = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_context.GetPhysicalDevice(), format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    LOG_ERROR("Failed to find supported depth format!");
    throw std::runtime_error("Failed to find supported depth format!");
}

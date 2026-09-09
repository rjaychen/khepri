#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <optional>
#include <memory>
#include <functional>
#include "PhysicalDevice.h"
#include "VulkanQueue.h"

using QueueFamilyIndices = khepri::QueueFamilyIndices;
using VulkanPhysicalDevice = khepri::VulkanPhysicalDevice;
using VulkanQueue = khepri::VulkanQueue;

class VulkanContext {
public:
    VulkanContext(GLFWwindow* window, bool enableValidationLayers = true);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VkInstance GetInstance() const { return m_instance; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice.GetHandle(); }
    const khepri::VulkanPhysicalDevice& GetPhysicalDeviceInfo() const { return m_physicalDevice; }
    const std::vector<khepri::VulkanPhysicalDevice>& GetAvailablePhysicalDevices() const { return m_availableDevices; }

    VkDevice GetDevice() const { return m_device; }
    VkSurfaceKHR GetSurface() const { return m_surface; }
    const khepri::VulkanQueue& GetGraphicsQueue() const { return *m_graphicsQueue; }
    khepri::VulkanQueue& GetGraphicsQueue() { return *m_graphicsQueue; }

    const khepri::VulkanQueue& GetPresentQueue() const { return *m_presentQueue; }
    khepri::VulkanQueue& GetPresentQueue() { return *m_presentQueue; }

    const khepri::VulkanQueue& GetComputeQueue() const { return *m_computeQueue; }
    khepri::VulkanQueue& GetComputeQueue() { return *m_computeQueue; }

    VkQueue GetGraphicsQueueHandle() const { return m_graphicsQueue ? m_graphicsQueue->GetHandle() : VK_NULL_HANDLE; }
    VkQueue GetPresentQueueHandle() const { return m_presentQueue ? m_presentQueue->GetHandle() : VK_NULL_HANDLE; }
    VkQueue GetComputeQueueHandle() const { return m_computeQueue ? m_computeQueue->GetHandle() : VK_NULL_HANDLE; }
    khepri::QueueFamilyIndices GetQueueFamilies() const { return m_physicalDevice.GetQueueFamilies(); }
    VmaAllocator GetAllocator() const { return m_allocator; }
    VkPhysicalDeviceProperties GetDeviceProperties() const { return m_physicalDevice.GetProperties(); }
    VkSampleCountFlagBits GetMaxUsableSampleCount() const {
        VkSampleCountFlags counts = m_physicalDevice.GetProperties().limits.framebufferColorSampleCounts &
                                    m_physicalDevice.GetProperties().limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
        if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
        if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
        return VK_SAMPLE_COUNT_1_BIT;
    }

    void WaitIdle() const { vkDeviceWaitIdle(m_device); }
    void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& action) const;

    [[nodiscard]] VkCommandPool CreateCommandPool(
        VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        std::optional<uint32_t> queueFamilyIndex = std::nullopt) const;

    [[nodiscard]] std::vector<VkCommandBuffer> AllocateCommandBuffers(
        VkCommandPool commandPool,
        uint32_t count,
        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;

    [[nodiscard]] VkCommandBuffer AllocateCommandBuffer(
        VkCommandPool commandPool,
        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;

private:
    void InitVolk();
    void CreateInstance(bool enableValidation);
    void SetupDebugMessenger();
    void CreateSurface(GLFWwindow* window);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateAllocator();

    bool CheckValidationLayerSupport();
    bool CheckInstanceExtensionSupport(const char* extName);

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    khepri::VulkanPhysicalDevice m_physicalDevice;
    std::vector<khepri::VulkanPhysicalDevice> m_availableDevices;

    VkDevice m_device = VK_NULL_HANDLE;

    std::unique_ptr<khepri::VulkanQueue> m_graphicsQueue;
    std::unique_ptr<khepri::VulkanQueue> m_presentQueue;
    std::unique_ptr<khepri::VulkanQueue> m_computeQueue;

    VmaAllocator m_allocator = VK_NULL_HANDLE;

    bool m_enableValidationLayers = true;
    std::vector<const char*> m_validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    std::vector<const char*> m_deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
};

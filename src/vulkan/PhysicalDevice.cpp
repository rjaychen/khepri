#include "PhysicalDevice.h"
#include "VulkanUtils.h"
#include "../core/Logger.h"
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <set>

namespace Khepri {

VulkanPhysicalDevice::VulkanPhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface)
    : m_handle(device) {
    if (m_handle == VK_NULL_HANDLE) return;

    QueryProperties();
    QueryFeatures();
    QueryMemoryProperties();
    QueryQueueFamilies(surface);
    QueryExtensions();
    QuerySwapchainSupport(surface);
}

void VulkanPhysicalDevice::QueryProperties() {
    vkGetPhysicalDeviceProperties(m_handle, &m_properties);
}

void VulkanPhysicalDevice::QueryFeatures() {
    VkPhysicalDeviceVulkan13Features v13{};
    v13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &v13;

    if (vkGetPhysicalDeviceFeatures2 != nullptr) {
        vkGetPhysicalDeviceFeatures2(m_handle, &features2);
        m_features = features2.features;
        m_vulkan13Features = v13;
    } else {
        vkGetPhysicalDeviceFeatures(m_handle, &m_features);
    }
}

void VulkanPhysicalDevice::QueryMemoryProperties() {
    vkGetPhysicalDeviceMemoryProperties(m_handle, &m_memoryProperties);
}

void VulkanPhysicalDevice::QueryQueueFamilies(VkSurfaceKHR surface) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_queueFamilies.graphicsFamily = i;
        }
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            m_queueFamilies.computeFamily = i;
        }

        VkBool32 presentSupport = false;
        if (surface != VK_NULL_HANDLE) {
            vkGetPhysicalDeviceSurfaceSupportKHR(m_handle, i, surface, &presentSupport);
        } else {
            presentSupport = (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) ? VK_TRUE : VK_FALSE;
        }

        if (presentSupport) {
            m_queueFamilies.presentFamily = i;
        }

        if (m_queueFamilies.isComplete()) break;
        i++;
    }
}

void VulkanPhysicalDevice::QueryExtensions() {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(m_handle, nullptr, &extensionCount, nullptr);

    m_availableExtensions.resize(extensionCount);
    vkEnumerateDeviceExtensionProperties(m_handle, nullptr, &extensionCount, m_availableExtensions.data());
}

void VulkanPhysicalDevice::QuerySwapchainSupport(VkSurfaceKHR surface) {
    if (surface == VK_NULL_HANDLE) return;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_handle, surface, &m_swapchainSupport.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_handle, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        m_swapchainSupport.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_handle, surface, &formatCount, m_swapchainSupport.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_handle, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        m_swapchainSupport.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_handle, surface, &presentModeCount, m_swapchainSupport.presentModes.data());
    }
}

uint64_t VulkanPhysicalDevice::GetDedicatedVRAMBytes() const {
    uint64_t totalBytes = 0;
    for (uint32_t i = 0; i < m_memoryProperties.memoryHeapCount; ++i) {
        if (m_memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalBytes += m_memoryProperties.memoryHeaps[i].size;
        }
    }
    return totalBytes;
}

float VulkanPhysicalDevice::GetDedicatedVRAMGigabytes() const {
    return static_cast<float>(GetDedicatedVRAMBytes()) / (1024.0f * 1024.0f * 1024.0f);
}

bool VulkanPhysicalDevice::IsExtensionSupported(const char* extensionName) const {
    if (!extensionName) return false;
    for (const auto& ext : m_availableExtensions) {
        if (std::strcmp(extensionName, ext.extensionName) == 0) {
            return true;
        }
    }
    return false;
}

bool VulkanPhysicalDevice::SupportsSwapchain() const {
    return IsExtensionSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

bool VulkanPhysicalDevice::IsSuitable(const std::vector<const char*>& requiredExtensions) const {
    if (!m_queueFamilies.isComplete()) {
        return false;
    }

    for (const char* reqExt : requiredExtensions) {
        if (!IsExtensionSupported(reqExt)) {
            return false;
        }
    }

    if (!m_features.samplerAnisotropy || !m_features.fillModeNonSolid) {
        return false;
    }

    if (!m_vulkan13Features.dynamicRendering || !m_vulkan13Features.synchronization2) {
        return false;
    }

    return true;
}

int VulkanPhysicalDevice::CalculateScore(const std::vector<const char*>& requiredExtensions) const {
    if (!IsSuitable(requiredExtensions)) {
        return -1;
    }

    int score = 0;

    switch (m_properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 10000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 1000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 500;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score += 100;
            break;
        default:
            score += 50;
            break;
    }

    // Weight score by dedicated VRAM (1 point per 10 MB)
    uint64_t vramMB = GetDedicatedVRAMBytes() / (1024 * 1024);
    score += static_cast<int>(vramMB / 10);

    // Minor weight for max texture size
    score += static_cast<int>(m_properties.limits.maxImageDimension2D / 1024);

    return score;
}

std::vector<VulkanPhysicalDevice> VulkanPhysicalDevice::Enumerate(VkInstance instance, VkSurfaceKHR surface) {
    std::vector<VulkanPhysicalDevice> physicalDevices;
    if (instance == VK_NULL_HANDLE) return physicalDevices;

    uint32_t deviceCount = 0;
    VkResult res = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    CHECK_VK_RESULT(res, "Failed to enumerate physical devices count");

    if (deviceCount == 0) {
        LOG_WARN("No physical GPU devices with Vulkan support found!");
        return physicalDevices;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    res = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    CHECK_VK_RESULT(res, "Failed to enumerate physical devices list");

    physicalDevices.reserve(deviceCount);
    for (VkPhysicalDevice dev : devices) {
        physicalDevices.emplace_back(dev, surface);
    }

    return physicalDevices;
}

VulkanPhysicalDevice VulkanPhysicalDevice::SelectBest(const std::vector<VulkanPhysicalDevice>& devices,
                                                     const std::vector<const char*>& requiredExtensions,
                                                     int preferredIndex) {
    if (devices.empty()) {
        LOG_ERROR("Cannot select physical device: device list is empty!");
        throw std::runtime_error("No Vulkan physical devices found!");
    }

    if (preferredIndex >= 0 && preferredIndex < static_cast<int>(devices.size())) {
        if (devices[preferredIndex].IsSuitable(requiredExtensions)) {
            LOG_INFO("Selected user-preferred GPU: " + devices[preferredIndex].GetDeviceName());
            return devices[preferredIndex];
        }
        LOG_WARN("Preferred GPU index " + std::to_string(preferredIndex) + " is not suitable, falling back to auto-scoring.");
    }

    int bestScore = -1;
    const VulkanPhysicalDevice* bestDevice = nullptr;

    for (const auto& device : devices) {
        int score = device.CalculateScore(requiredExtensions);
        LOG_INFO("Evaluating GPU: " + device.GetDeviceName() + 
                 " | Type: " + std::to_string(device.GetDeviceType()) + 
                 " | Dedicated VRAM: " + std::to_string(device.GetDedicatedVRAMGigabytes()) + " GB" +
                 " | Score: " + std::to_string(score));

        if (score > bestScore) {
            bestScore = score;
            bestDevice = &device;
        }
    }

    if (!bestDevice || bestScore < 0) {
        LOG_ERROR("Failed to find a GPU that satisfies all required Vulkan 1.3 features and extensions!");
        throw std::runtime_error("Failed to find a suitable physical device!");
    }

    LOG_INFO("Selected best GPU: " + bestDevice->GetDeviceName() + 
             " (Score: " + std::to_string(bestScore) + ")");
    return *bestDevice;
}

} // namespace Khepri

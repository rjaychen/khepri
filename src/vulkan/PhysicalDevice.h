#pragma once

#include <volk.h>
#include <vector>
#include <string>
#include <optional>
#include <cstdint>

namespace khepri {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value();
    }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;

    bool IsAdequate() const {
        return !formats.empty() && !presentModes.empty();
    }
};

class VulkanPhysicalDevice {
public:
    VulkanPhysicalDevice() = default;
    explicit VulkanPhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface = VK_NULL_HANDLE);

    VkPhysicalDevice GetHandle() const { return m_handle; }
    const VkPhysicalDeviceProperties& GetProperties() const { return m_properties; }
    const VkPhysicalDeviceFeatures& GetFeatures() const { return m_features; }
    const VkPhysicalDeviceVulkan13Features& GetVulkan13Features() const { return m_vulkan13Features; }
    const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_memoryProperties; }
    const QueueFamilyIndices& GetQueueFamilies() const { return m_queueFamilies; }
    const SwapchainSupportDetails& GetSwapchainSupport() const { return m_swapchainSupport; }
    const std::vector<VkExtensionProperties>& GetAvailableExtensions() const { return m_availableExtensions; }

    std::string GetDeviceName() const { return m_properties.deviceName; }
    VkPhysicalDeviceType GetDeviceType() const { return m_properties.deviceType; }
    uint32_t GetDriverVersion() const { return m_properties.driverVersion; }
    uint32_t GetApiVersion() const { return m_properties.apiVersion; }
    uint64_t GetDedicatedVRAMBytes() const;
    float GetDedicatedVRAMGigabytes() const;

    bool IsDiscreteGPU() const { return m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU; }
    bool IsIntegratedGPU() const { return m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU; }
    bool IsExtensionSupported(const char* extensionName) const;
    bool SupportsSwapchain() const;
    bool IsSuitable(const std::vector<const char*>& requiredExtensions) const;

    int CalculateScore(const std::vector<const char*>& requiredExtensions) const;

    static std::vector<VulkanPhysicalDevice> Enumerate(VkInstance instance, VkSurfaceKHR surface = VK_NULL_HANDLE);
    static VulkanPhysicalDevice SelectBest(const std::vector<VulkanPhysicalDevice>& devices,
                                           const std::vector<const char*>& requiredExtensions,
                                           int preferredIndex = -1);

private:
    void QueryProperties();
    void QueryFeatures();
    void QueryMemoryProperties();
    void QueryQueueFamilies(VkSurfaceKHR surface);
    void QueryExtensions();
    void QuerySwapchainSupport(VkSurfaceKHR surface);

    VkPhysicalDevice m_handle = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties m_properties{};
    VkPhysicalDeviceFeatures m_features{};
    VkPhysicalDeviceVulkan13Features m_vulkan13Features{};
    VkPhysicalDeviceMemoryProperties m_memoryProperties{};
    QueueFamilyIndices m_queueFamilies;
    SwapchainSupportDetails m_swapchainSupport;
    std::vector<VkExtensionProperties> m_availableExtensions;
};

} // namespace khepri

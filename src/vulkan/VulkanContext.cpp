#define VMA_IMPLEMENTATION
#include "VulkanContext.h"
#include "VulkanUtils.h"
#include "../core/Logger.h"
#include <set>
#include <stdexcept>
#include <cstring>
#include <algorithm>

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    (void)messageType;
    (void)pUserData;

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LOG_ERROR(std::string("[Vulkan Validation] ") + pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_WARN(std::string("[Vulkan Validation] ") + pCallbackData->pMessage);
    } else {
        LOG_VULKAN(std::string("[Vulkan Debug] ") + pCallbackData->pMessage);
    }

    return VK_FALSE;
}

VulkanContext::VulkanContext(GLFWwindow* window, bool enableValidationLayers)
    : m_enableValidationLayers(enableValidationLayers) {
    InitVolk();
    CreateInstance(enableValidationLayers);
    SetupDebugMessenger();
    CreateSurface(window);
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateAllocator();

    LOG_INFO("VulkanContext initialized successfully. Device: " + m_physicalDevice.GetDeviceName());
}

VulkanContext::~VulkanContext() {
    if (m_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_allocator);
    }

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }

    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            func(m_instance, m_debugMessenger, nullptr);
        }
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }
}

bool VulkanContext::CheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : m_validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

bool VulkanContext::CheckInstanceExtensionSupport(const char* extName) {
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data());
    for (const auto& e : available) {
        if (std::strcmp(extName, e.extensionName) == 0) return true;
    }
    return false;
}

void VulkanContext::InitVolk() {
    VkResult res = volkInitialize();
    CHECK_VK_RESULT(res, "Failed to initialize Volk meta-loader");
    LOG_INFO("Volk meta-loader initialized successfully");
}

void VulkanContext::CreateInstance(bool enableValidation) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Khepri Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "KhepriEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // --- Runtime availability checks (layers + debug extension) ---
    bool validationAvailable = enableValidation && CheckValidationLayerSupport();
    bool debugUtilsAvailable = validationAvailable &&
                               CheckInstanceExtensionSupport(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    if (enableValidation && !validationAvailable) {
        LOG_WARN("VK_LAYER_KHRONOS_validation not found. "
                 "Install the Vulkan SDK to enable GPU validation. Running without it.");
    }
    m_enableValidationLayers = validationAvailable;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions;
    if (glfwExtensions && glfwExtensionCount > 0) {
        extensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);
    }

    if (debugUtilsAvailable) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // --- OS-Specific Platform Extensions & Portability Flags ---
#if defined(__APPLE__) || defined(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)
    if (CheckInstanceExtensionSupport("VK_KHR_portability_enumeration")) {
        extensions.push_back("VK_KHR_portability_enumeration");
        createInfo.flags |= 0x00000001; // VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
    }
#endif
    if (CheckInstanceExtensionSupport(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (validationAvailable) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VkResult res = vkCreateInstance(&createInfo, nullptr, &m_instance);
    CHECK_VK_RESULT(res, "Failed to create Vulkan Instance");

    volkLoadInstance(m_instance);
}

void VulkanContext::SetupDebugMessenger() {
    if (!m_enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) {
        VkResult res = func(m_instance, &createInfo, nullptr, &m_debugMessenger);
        CHECK_VK_RESULT(res, "Failed to set up Vulkan Debug Messenger");
    }
}

void VulkanContext::CreateSurface(GLFWwindow* window) {
    if (!window) return;
    VkResult res = glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface);
    CHECK_VK_RESULT(res, "Failed to create GLFW window surface");
}

void VulkanContext::PickPhysicalDevice() {
    m_availableDevices = Khepri::VulkanPhysicalDevice::Enumerate(m_instance, m_surface);
    if (m_availableDevices.empty()) {
        LOG_ERROR("Failed to find GPUs with Vulkan support!");
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    m_physicalDevice = Khepri::VulkanPhysicalDevice::SelectBest(m_availableDevices, m_deviceExtensions);
}

void VulkanContext::CreateLogicalDevice() {
    const auto& queueFamilies = m_physicalDevice.GetQueueFamilies();
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        queueFamilies.graphicsFamily.value(),
        queueFamilies.presentFamily.value(),
        queueFamilies.computeFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE; // Enable wireframe rendering for computational geometry!

    // Vulkan 1.3 features (Dynamic Rendering & Synchronization2)
    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.dynamicRendering = VK_TRUE;
    vulkan13Features.synchronization2 = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &vulkan13Features;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

    // Device layers are deprecated since Vulkan 1.0 and forbidden by spec (VUID-VkDeviceCreateInfo-enabledLayerCount-12384).
    // Only instance layers should be used. Always set to 0 here.
    createInfo.enabledLayerCount = 0;

    VkResult res = vkCreateDevice(m_physicalDevice.GetHandle(), &createInfo, nullptr, &m_device);
    CHECK_VK_RESULT(res, "Failed to create Vulkan logical device");

    volkLoadDevice(m_device);

    vkGetDeviceQueue(m_device, queueFamilies.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, queueFamilies.presentFamily.value(), 0, &m_presentQueue);
    vkGetDeviceQueue(m_device, queueFamilies.computeFamily.value(), 0, &m_computeQueue);
}

void VulkanContext::CreateAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.physicalDevice = m_physicalDevice.GetHandle();
    allocatorInfo.device = m_device;
    allocatorInfo.instance = m_instance;

    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;

    VkResult res = vmaCreateAllocator(&allocatorInfo, &m_allocator);
    CHECK_VK_RESULT(res, "Failed to create Vulkan Memory Allocator (VMA)");
}

VkCommandPool VulkanContext::CreateCommandPool(VkCommandPoolCreateFlags flags, std::optional<uint32_t> queueFamilyIndex) const {
    const uint32_t familyIndex = queueFamilyIndex.value_or(m_physicalDevice.GetQueueFamilies().graphicsFamily.value());
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = flags;
    poolInfo.queueFamilyIndex = familyIndex;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    const VkResult res = vkCreateCommandPool(m_device, &poolInfo, nullptr, &commandPool);
    CHECK_VK_RESULT(res, "Failed to create Vulkan command pool");
    return commandPool;
}

std::vector<VkCommandBuffer> VulkanContext::AllocateCommandBuffers(VkCommandPool commandPool, uint32_t count, VkCommandBufferLevel level) const {
    if (count == 0) return {};

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = level;
    allocInfo.commandBufferCount = count;

    std::vector<VkCommandBuffer> commandBuffers(count, VK_NULL_HANDLE);
    const VkResult res = vkAllocateCommandBuffers(m_device, &allocInfo, commandBuffers.data());
    CHECK_VK_RESULT(res, "Failed to allocate Vulkan command buffers");
    return commandBuffers;
}

VkCommandBuffer VulkanContext::AllocateCommandBuffer(VkCommandPool commandPool, VkCommandBufferLevel level) const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = level;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    const VkResult res = vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);
    CHECK_VK_RESULT(res, "Failed to allocate Vulkan command buffer");
    return commandBuffer;
}

void VulkanContext::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& action) const {
    if (!action) return;

    const VkCommandPool commandPool = CreateCommandPool(VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    const VkCommandBuffer cmd = AllocateCommandBuffer(commandPool);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult res = vkBeginCommandBuffer(cmd, &beginInfo);
    CHECK_VK_RESULT(res, "Failed to begin command buffer in ImmediateSubmit");
    action(cmd);
    res = vkEndCommandBuffer(cmd);
    CHECK_VK_RESULT(res, "Failed to end command buffer in ImmediateSubmit");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    res = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    CHECK_VK_RESULT(res, "Failed to submit command buffer in ImmediateSubmit");
    res = vkQueueWaitIdle(m_graphicsQueue);
    CHECK_VK_RESULT(res, "Failed to wait for queue idle in ImmediateSubmit");

    vkFreeCommandBuffers(m_device, commandPool, 1, &cmd);
    vkDestroyCommandPool(m_device, commandPool, nullptr);
}

#pragma once

#include <volk.h>
#include <stdexcept>
#include "VulkanUtils.h"

namespace khepri {

/**
 * @brief Lean move-only RAII wrapper for VkSemaphore.
 */
class VulkanSemaphore {
public:
    VulkanSemaphore() noexcept = default;

    explicit VulkanSemaphore(VkDevice device)
        : m_device(device)
    {
        if (m_device == VK_NULL_HANDLE) {
            throw std::invalid_argument("Cannot create VulkanSemaphore with VK_NULL_HANDLE device");
        }
        VkSemaphoreCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkResult res = vkCreateSemaphore(m_device, &createInfo, nullptr, &m_handle);
        CHECK_VK_RESULT(res, "Failed to create Vulkan semaphore");
    }

    ~VulkanSemaphore() noexcept {
        Destroy();
    }

    VulkanSemaphore(const VulkanSemaphore&) = delete;
    VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

    VulkanSemaphore(VulkanSemaphore&& other) noexcept
        : m_device(other.m_device), m_handle(other.m_handle)
    {
        other.m_device = VK_NULL_HANDLE;
        other.m_handle = VK_NULL_HANDLE;
    }

    VulkanSemaphore& operator=(VulkanSemaphore&& other) noexcept {
        if (this != &other) {
            Destroy();
            m_device = other.m_device;
            m_handle = other.m_handle;
            other.m_device = VK_NULL_HANDLE;
            other.m_handle = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Destroy() noexcept {
        if (m_handle != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, m_handle, nullptr);
            m_handle = VK_NULL_HANDLE;
            m_device = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkSemaphore GetHandle() const noexcept { return m_handle; }
    [[nodiscard]] operator VkSemaphore() const noexcept { return m_handle; }
    [[nodiscard]] VkDevice GetDevice() const noexcept { return m_device; }
    [[nodiscard]] bool IsValid() const noexcept { return m_handle != VK_NULL_HANDLE; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkSemaphore m_handle = VK_NULL_HANDLE;
};

/**
 * @brief Lean move-only RAII wrapper for VkFence.
 */
class VulkanFence {
public:
    VulkanFence() noexcept = default;

    explicit VulkanFence(VkDevice device, VkFenceCreateFlags flags = 0)
        : m_device(device)
    {
        if (m_device == VK_NULL_HANDLE) {
            throw std::invalid_argument("Cannot create VulkanFence with VK_NULL_HANDLE device");
        }
        VkFenceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        createInfo.flags = flags;
        VkResult res = vkCreateFence(m_device, &createInfo, nullptr, &m_handle);
        CHECK_VK_RESULT(res, "Failed to create Vulkan fence");
    }

    ~VulkanFence() noexcept {
        Destroy();
    }

    VulkanFence(const VulkanFence&) = delete;
    VulkanFence& operator=(const VulkanFence&) = delete;

    VulkanFence(VulkanFence&& other) noexcept
        : m_device(other.m_device), m_handle(other.m_handle)
    {
        other.m_device = VK_NULL_HANDLE;
        other.m_handle = VK_NULL_HANDLE;
    }

    VulkanFence& operator=(VulkanFence&& other) noexcept {
        if (this != &other) {
            Destroy();
            m_device = other.m_device;
            m_handle = other.m_handle;
            other.m_device = VK_NULL_HANDLE;
            other.m_handle = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Destroy() noexcept {
        if (m_handle != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, m_handle, nullptr);
            m_handle = VK_NULL_HANDLE;
            m_device = VK_NULL_HANDLE;
        }
    }

    VkResult Wait(uint64_t timeoutNanoseconds = UINT64_MAX) const {
        if (m_handle == VK_NULL_HANDLE || m_device == VK_NULL_HANDLE) {
            throw std::runtime_error("Attempted to wait on invalid VulkanFence");
        }
        return vkWaitForFences(m_device, 1, &m_handle, VK_TRUE, timeoutNanoseconds);
    }

    VkResult Reset() {
        if (m_handle == VK_NULL_HANDLE || m_device == VK_NULL_HANDLE) {
            throw std::runtime_error("Attempted to reset invalid VulkanFence");
        }
        return vkResetFences(m_device, 1, &m_handle);
    }

    [[nodiscard]] bool IsSignaled() const {
        if (m_handle == VK_NULL_HANDLE || m_device == VK_NULL_HANDLE) {
            return false;
        }
        return vkGetFenceStatus(m_device, m_handle) == VK_SUCCESS;
    }

    [[nodiscard]] VkFence GetHandle() const noexcept { return m_handle; }
    [[nodiscard]] operator VkFence() const noexcept { return m_handle; }
    [[nodiscard]] VkDevice GetDevice() const noexcept { return m_device; }
    [[nodiscard]] bool IsValid() const noexcept { return m_handle != VK_NULL_HANDLE; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkFence m_handle = VK_NULL_HANDLE;
};

} // namespace khepri

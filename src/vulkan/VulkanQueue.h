#pragma once

#include <volk.h>
#include <mutex>
#include <vector>
#include <span>
#include <cstdint>

namespace khepri {

/**
 * @brief High-level descriptor for configuring a single-buffer submission with Vulkan 1.3 synchronization2.
 */
struct QueueSubmitDescriptor {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    VkSemaphore waitSemaphore = VK_NULL_HANDLE;
    VkPipelineStageFlags2 waitStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    uint64_t waitValue = 0; // For timeline semaphores

    VkSemaphore signalSemaphore = VK_NULL_HANDLE;
    VkPipelineStageFlags2 signalStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    uint64_t signalValue = 0; // For timeline semaphores

    VkFence fence = VK_NULL_HANDLE;
};

/**
 * @brief Thread-safe wrapper encapsulating a VkQueue handle, family index, capabilities,
 *        and modern Vulkan 1.3 synchronization2 submission methods.
 */
class VulkanQueue {
public:
    VulkanQueue() = default;
    VulkanQueue(VkDevice device, VkQueue queue, uint32_t familyIndex, uint32_t queueIndex, VkQueueFlags flags);
    ~VulkanQueue() = default;

    // Non-copyable (hardware execution port with mutex)
    VulkanQueue(const VulkanQueue&) = delete;
    VulkanQueue& operator=(const VulkanQueue&) = delete;

    // Movable
    VulkanQueue(VulkanQueue&& other) noexcept;
    VulkanQueue& operator=(VulkanQueue&& other) noexcept;

    // Getters & Query properties
    [[nodiscard]] VkQueue GetHandle() const noexcept { return m_queue; }
    [[nodiscard]] VkDevice GetDevice() const noexcept { return m_device; }
    [[nodiscard]] uint32_t GetFamilyIndex() const noexcept { return m_familyIndex; }
    [[nodiscard]] uint32_t GetQueueIndex() const noexcept { return m_queueIndex; }
    [[nodiscard]] VkQueueFlags GetFlags() const noexcept { return m_flags; }
    [[nodiscard]] bool IsValid() const noexcept { return m_queue != VK_NULL_HANDLE; }

    [[nodiscard]] bool SupportsGraphics() const noexcept { return (m_flags & VK_QUEUE_GRAPHICS_BIT) != 0; }
    [[nodiscard]] bool SupportsCompute() const noexcept { return (m_flags & VK_QUEUE_COMPUTE_BIT) != 0; }
    [[nodiscard]] bool SupportsTransfer() const noexcept { return (m_flags & VK_QUEUE_TRANSFER_BIT) != 0; }
    [[nodiscard]] bool SupportsSparseBinding() const noexcept { return (m_flags & VK_QUEUE_SPARSE_BINDING_BIT) != 0; }

    /// Implicit conversion to raw VkQueue for C API interoperability
    operator VkQueue() const noexcept { return m_queue; }

    // --- Thread-Safe Execution & Synchronization ---

    /**
     * @brief Submit one or more batches using Vulkan 1.3 vkQueueSubmit2 under mutex lock.
     */
    VkResult Submit2(std::span<const VkSubmitInfo2> submits, VkFence fence = VK_NULL_HANDLE) const;

    /**
     * @brief Submit a single submit info using Vulkan 1.3 vkQueueSubmit2.
     */
    VkResult Submit2(const VkSubmitInfo2& submitInfo, VkFence fence = VK_NULL_HANDLE) const;

    /**
     * @brief High-level helper to submit a single command buffer with optional wait/signal semaphores and fence.
     */
    VkResult Submit(const QueueSubmitDescriptor& desc) const;

    /**
     * @brief Synchronously submits a command buffer using vkQueueSubmit2 and waits for queue idle.
     */
    VkResult SubmitAndWait(VkCommandBuffer commandBuffer) const;

    /**
     * @brief Thread-safe presentation of a swapchain image using vkQueuePresentKHR.
     */
    VkResult Present(const VkPresentInfoKHR& presentInfo) const;

    /**
     * @brief Waits until the queue has finished all pending work.
     */
    VkResult WaitIdle() const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_familyIndex = 0;
    uint32_t m_queueIndex = 0;
    VkQueueFlags m_flags = 0;
    mutable std::mutex m_submitMutex;
};

} // namespace khepri

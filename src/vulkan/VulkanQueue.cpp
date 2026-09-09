#include "VulkanQueue.h"
#include "VulkanUtils.h"

namespace khepri {

VulkanQueue::VulkanQueue(VkDevice device, VkQueue queue, uint32_t familyIndex, uint32_t queueIndex, VkQueueFlags flags)
    : m_device(device), m_queue(queue), m_familyIndex(familyIndex), m_queueIndex(queueIndex), m_flags(flags) {}

VulkanQueue::VulkanQueue(VulkanQueue&& other) noexcept {
    std::scoped_lock lock(other.m_submitMutex);
    m_device = other.m_device;
    m_queue = other.m_queue;
    m_familyIndex = other.m_familyIndex;
    m_queueIndex = other.m_queueIndex;
    m_flags = other.m_flags;
    other.m_queue = VK_NULL_HANDLE;
    other.m_device = VK_NULL_HANDLE;
}

VulkanQueue& VulkanQueue::operator=(VulkanQueue&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(m_submitMutex, other.m_submitMutex);
        m_device = other.m_device;
        m_queue = other.m_queue;
        m_familyIndex = other.m_familyIndex;
        m_queueIndex = other.m_queueIndex;
        m_flags = other.m_flags;
        other.m_queue = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }
    return *this;
}

VkResult VulkanQueue::Submit2(std::span<const VkSubmitInfo2> submits, VkFence fence) const {
    if (m_queue == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
    std::lock_guard<std::mutex> lock(m_submitMutex);
    return vkQueueSubmit2(m_queue, static_cast<uint32_t>(submits.size()), submits.data(), fence);
}

VkResult VulkanQueue::Submit2(const VkSubmitInfo2& submitInfo, VkFence fence) const {
    return Submit2(std::span<const VkSubmitInfo2>(&submitInfo, 1), fence);
}

VkResult VulkanQueue::Submit(const QueueSubmitDescriptor& desc) const {
    if (m_queue == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;

    VkCommandBufferSubmitInfo cmdInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
    cmdInfo.commandBuffer = desc.commandBuffer;

    VkSemaphoreSubmitInfo waitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
    if (desc.waitSemaphore != VK_NULL_HANDLE) {
        waitInfo.semaphore = desc.waitSemaphore;
        waitInfo.stageMask = desc.waitStageMask;
        waitInfo.value = desc.waitValue;
    }

    VkSemaphoreSubmitInfo signalInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
    if (desc.signalSemaphore != VK_NULL_HANDLE) {
        signalInfo.semaphore = desc.signalSemaphore;
        signalInfo.stageMask = desc.signalStageMask;
        signalInfo.value = desc.signalValue;
    }

    VkSubmitInfo2 submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
    submitInfo.commandBufferInfoCount = (desc.commandBuffer != VK_NULL_HANDLE) ? 1 : 0;
    submitInfo.pCommandBufferInfos = (desc.commandBuffer != VK_NULL_HANDLE) ? &cmdInfo : nullptr;

    submitInfo.waitSemaphoreInfoCount = (desc.waitSemaphore != VK_NULL_HANDLE) ? 1 : 0;
    submitInfo.pWaitSemaphoreInfos = (desc.waitSemaphore != VK_NULL_HANDLE) ? &waitInfo : nullptr;

    submitInfo.signalSemaphoreInfoCount = (desc.signalSemaphore != VK_NULL_HANDLE) ? 1 : 0;
    submitInfo.pSignalSemaphoreInfos = (desc.signalSemaphore != VK_NULL_HANDLE) ? &signalInfo : nullptr;

    return Submit2(std::span<const VkSubmitInfo2>(&submitInfo, 1), desc.fence);
}

VkResult VulkanQueue::SubmitAndWait(VkCommandBuffer commandBuffer) const {
    if (m_queue == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;

    std::lock_guard<std::mutex> lock(m_submitMutex);
    VkCommandBufferSubmitInfo cmdInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
    cmdInfo.commandBuffer = commandBuffer;

    VkSubmitInfo2 submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
    submitInfo.commandBufferInfoCount = (commandBuffer != VK_NULL_HANDLE) ? 1 : 0;
    submitInfo.pCommandBufferInfos = (commandBuffer != VK_NULL_HANDLE) ? &cmdInfo : nullptr;

    VkResult res = vkQueueSubmit2(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (res != VK_SUCCESS) return res;

    return vkQueueWaitIdle(m_queue);
}

VkResult VulkanQueue::Present(const VkPresentInfoKHR& presentInfo) const {
    if (m_queue == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
    std::lock_guard<std::mutex> lock(m_submitMutex);
    return vkQueuePresentKHR(m_queue, &presentInfo);
}

VkResult VulkanQueue::WaitIdle() const {
    if (m_queue == VK_NULL_HANDLE) return VK_SUCCESS;
    std::lock_guard<std::mutex> lock(m_submitMutex);
    return vkQueueWaitIdle(m_queue);
}

} // namespace khepri

#pragma once

#include <volk.h>
#include <exception>
#include <stdexcept>
#include "VulkanUtils.h"
#include "../core/Logger.h"

namespace khepri {

/**
 * @brief Scoped RAII guard for VkCommandBuffer recording.
 * Begins recording on construction and ends on destruction.
 * Safely skips ending if an exception is unwinding to abandon broken command streams.
 */
class ScopedCommandBuffer {
public:
    explicit ScopedCommandBuffer(
        VkCommandBuffer cmd,
        VkCommandBufferUsageFlags flags = 0,
        const VkCommandBufferInheritanceInfo* pInheritanceInfo = nullptr)
        : m_commandBuffer(cmd)
    {
        if (m_commandBuffer == VK_NULL_HANDLE) {
            throw std::invalid_argument("ScopedCommandBuffer cannot record a VK_NULL_HANDLE command buffer");
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = flags;
        beginInfo.pInheritanceInfo = pInheritanceInfo;

        VkResult res = vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
        CHECK_VK_RESULT(res, "Failed to begin command buffer recording in ScopedCommandBuffer");
        m_isRecording = true;
    }

    ~ScopedCommandBuffer() noexcept {
        if (m_isRecording && m_commandBuffer != VK_NULL_HANDLE) {
            if (!std::uncaught_exceptions()) {
                VkResult res = vkEndCommandBuffer(m_commandBuffer);
                if (res != VK_SUCCESS) {
                    LOG_ERROR("Failed to end command buffer recording in ~ScopedCommandBuffer: " +
                              std::string(VkResultToString(res)));
                }
            } else {
                LOG_WARN("ScopedCommandBuffer discarded due to active exception unwinding; recording not finalized.");
            }
            m_isRecording = false;
        }
    }

    ScopedCommandBuffer(const ScopedCommandBuffer&) = delete;
    ScopedCommandBuffer& operator=(const ScopedCommandBuffer&) = delete;

    ScopedCommandBuffer(ScopedCommandBuffer&& other) noexcept
        : m_commandBuffer(other.m_commandBuffer),
          m_isRecording(other.m_isRecording)
    {
        other.m_commandBuffer = VK_NULL_HANDLE;
        other.m_isRecording = false;
    }

    ScopedCommandBuffer& operator=(ScopedCommandBuffer&& other) noexcept {
        if (this != &other) {
            if (m_isRecording && m_commandBuffer != VK_NULL_HANDLE) {
                End();
            }
            m_commandBuffer = other.m_commandBuffer;
            m_isRecording = other.m_isRecording;
            other.m_commandBuffer = VK_NULL_HANDLE;
            other.m_isRecording = false;
        }
        return *this;
    }

    VkResult End() {
        if (!m_isRecording || m_commandBuffer == VK_NULL_HANDLE) {
            return VK_SUCCESS;
        }
        VkResult res = vkEndCommandBuffer(m_commandBuffer);
        m_isRecording = false;
        CHECK_VK_RESULT(res, "Failed to end command buffer recording in ScopedCommandBuffer::End");
        return res;
    }

    [[nodiscard]] VkCommandBuffer Get() const noexcept { return m_commandBuffer; }
    [[nodiscard]] operator VkCommandBuffer() const noexcept { return m_commandBuffer; }
    [[nodiscard]] bool IsRecording() const noexcept { return m_isRecording; }

private:
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    bool m_isRecording = false;
};

} // namespace khepri

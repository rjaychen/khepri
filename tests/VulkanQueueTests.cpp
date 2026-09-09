#include <gtest/gtest.h>
#include <volk.h>
#include "vulkan/VulkanQueue.h"

namespace {

TEST(VulkanQueueTest, DefaultConstructorInitializesNull) {
    khepri::VulkanQueue queue;
    EXPECT_EQ(queue.GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(queue.GetDevice(), VK_NULL_HANDLE);
    EXPECT_EQ(queue.GetFamilyIndex(), 0u);
    EXPECT_EQ(queue.GetQueueIndex(), 0u);
    EXPECT_EQ(queue.GetFlags(), 0u);
    EXPECT_FALSE(queue.IsValid());
    EXPECT_FALSE(queue.SupportsGraphics());
    EXPECT_FALSE(queue.SupportsCompute());
    EXPECT_FALSE(queue.SupportsTransfer());
    EXPECT_FALSE(queue.SupportsSparseBinding());
}

TEST(VulkanQueueTest, ParameterizedConstructorSetsProperties) {
    auto dummyDevice = reinterpret_cast<VkDevice>(0x1234);
    auto dummyQueue = reinterpret_cast<VkQueue>(0x5678);
    const uint32_t familyIndex = 2;
    const uint32_t queueIndex = 1;
    const VkQueueFlags flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

    khepri::VulkanQueue queue(dummyDevice, dummyQueue, familyIndex, queueIndex, flags);

    EXPECT_EQ(queue.GetHandle(), dummyQueue);
    EXPECT_EQ(queue.GetDevice(), dummyDevice);
    EXPECT_EQ(queue.GetFamilyIndex(), familyIndex);
    EXPECT_EQ(queue.GetQueueIndex(), queueIndex);
    EXPECT_EQ(queue.GetFlags(), flags);
    EXPECT_TRUE(queue.IsValid());
    EXPECT_TRUE(queue.SupportsGraphics());
    EXPECT_TRUE(queue.SupportsCompute());
    EXPECT_TRUE(queue.SupportsTransfer());
    EXPECT_FALSE(queue.SupportsSparseBinding());
    EXPECT_EQ(static_cast<VkQueue>(queue), dummyQueue);
}

TEST(VulkanQueueTest, MoveConstructorTransfersOwnership) {
    auto dummyDevice = reinterpret_cast<VkDevice>(0x1234);
    auto dummyQueue = reinterpret_cast<VkQueue>(0x5678);

    khepri::VulkanQueue original(dummyDevice, dummyQueue, 0, 0, VK_QUEUE_GRAPHICS_BIT);
    EXPECT_TRUE(original.IsValid());

    khepri::VulkanQueue moved(std::move(original));
    EXPECT_TRUE(moved.IsValid());
    EXPECT_EQ(moved.GetHandle(), dummyQueue);
    EXPECT_FALSE(original.IsValid());
    EXPECT_EQ(original.GetHandle(), VK_NULL_HANDLE);
}

TEST(VulkanQueueTest, MoveAssignmentTransfersOwnership) {
    auto dummyDevice = reinterpret_cast<VkDevice>(0x1234);
    auto dummyQueue = reinterpret_cast<VkQueue>(0x5678);

    khepri::VulkanQueue original(dummyDevice, dummyQueue, 1, 0, VK_QUEUE_COMPUTE_BIT);
    khepri::VulkanQueue target;

    target = std::move(original);
    EXPECT_TRUE(target.IsValid());
    EXPECT_EQ(target.GetHandle(), dummyQueue);
    EXPECT_TRUE(target.SupportsCompute());
    EXPECT_FALSE(original.IsValid());
}

TEST(VulkanQueueTest, UninitializedQueueMethodsReturnErrorGracefully) {
    khepri::VulkanQueue nullQueue;

    khepri::QueueSubmitDescriptor desc{};
    EXPECT_EQ(nullQueue.Submit(desc), VK_ERROR_INITIALIZATION_FAILED);
    EXPECT_EQ(nullQueue.SubmitAndWait(VK_NULL_HANDLE), VK_ERROR_INITIALIZATION_FAILED);

    VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    EXPECT_EQ(nullQueue.Present(presentInfo), VK_ERROR_INITIALIZATION_FAILED);
    EXPECT_EQ(nullQueue.WaitIdle(), VK_SUCCESS);
}

TEST(VulkanQueueTest, QueueSubmitDescriptorDefaultValues) {
    khepri::QueueSubmitDescriptor desc;
    EXPECT_EQ(desc.commandBuffer, VK_NULL_HANDLE);
    EXPECT_EQ(desc.waitSemaphore, VK_NULL_HANDLE);
    EXPECT_EQ(desc.waitStageMask, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    EXPECT_EQ(desc.waitValue, 0u);
    EXPECT_EQ(desc.signalSemaphore, VK_NULL_HANDLE);
    EXPECT_EQ(desc.signalStageMask, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    EXPECT_EQ(desc.signalValue, 0u);
    EXPECT_EQ(desc.fence, VK_NULL_HANDLE);
}

} // namespace

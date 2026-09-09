#include <gtest/gtest.h>
#include <volk.h>
#include "vulkan/CommandBuffer.h"
#include "vulkan/VulkanSync.h"

namespace {

TEST(CommandBufferTest, NullHandleThrowsInvalidArgument) {
    EXPECT_THROW({
        khepri::ScopedCommandBuffer scoped(VK_NULL_HANDLE);
    }, std::invalid_argument);
}

TEST(VulkanSyncTest, SemaphoreDefaultConstructor) {
    khepri::VulkanSemaphore sem;
    EXPECT_EQ(sem.GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(sem.GetDevice(), VK_NULL_HANDLE);
    EXPECT_FALSE(sem.IsValid());
    EXPECT_EQ(static_cast<VkSemaphore>(sem), VK_NULL_HANDLE);
    EXPECT_NO_THROW(sem.Destroy());
}

TEST(VulkanSyncTest, SemaphoreNullDeviceThrows) {
    EXPECT_THROW({
        khepri::VulkanSemaphore sem(VK_NULL_HANDLE);
    }, std::invalid_argument);
}

TEST(VulkanSyncTest, FenceDefaultConstructor) {
    khepri::VulkanFence fence;
    EXPECT_EQ(fence.GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(fence.GetDevice(), VK_NULL_HANDLE);
    EXPECT_FALSE(fence.IsValid());
    EXPECT_EQ(static_cast<VkFence>(fence), VK_NULL_HANDLE);
    EXPECT_FALSE(fence.IsSignaled());
    EXPECT_NO_THROW(fence.Destroy());
}

TEST(VulkanSyncTest, FenceNullDeviceThrows) {
    EXPECT_THROW({
        khepri::VulkanFence fence(VK_NULL_HANDLE);
    }, std::invalid_argument);
}

TEST(VulkanSyncTest, SemaphoreMoveSemantics) {
    khepri::VulkanSemaphore a;
    khepri::VulkanSemaphore b(std::move(a));
    EXPECT_FALSE(b.IsValid());
    EXPECT_EQ(b.GetHandle(), VK_NULL_HANDLE);
}

TEST(VulkanSyncTest, FenceMoveSemantics) {
    khepri::VulkanFence a;
    khepri::VulkanFence b(std::move(a));
    EXPECT_FALSE(b.IsValid());
    EXPECT_EQ(b.GetHandle(), VK_NULL_HANDLE);
}

} // namespace

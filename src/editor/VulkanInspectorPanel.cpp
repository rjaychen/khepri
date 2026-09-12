#include "VulkanInspectorPanel.h"

VulkanInspectorPanel::VulkanInspectorPanel(VulkanContext& context)
    : m_context(context) {}

void VulkanInspectorPanel::RenderUI(Swapchain& swapchain) {
    ImGui::Begin("Vulkan Educational Inspector");

    if (ImGui::CollapsingHeader("Physical Device & Driver", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& activeGPU = m_context.GetPhysicalDeviceInfo();
        auto props = activeGPU.GetProperties();

        const char* typeStr = "Unknown";
        switch (props.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: typeStr = "Discrete GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: typeStr = "Integrated GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: typeStr = "Virtual GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU: typeStr = "CPU"; break;
            default: break;
        }

        ImGui::Text("Active GPU: %s (%s)", props.deviceName, typeStr);
        ImGui::Text("Dedicated VRAM: %.2f GB", activeGPU.GetDedicatedVRAMGigabytes());
        ImGui::Text("API Version: %d.%d.%d", VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion));
        ImGui::Text("Driver Version: %d", props.driverVersion);
        ImGui::Text("Vendor ID: 0x%X | Device ID: 0x%X", props.vendorID, props.deviceID);

        const auto& allDevices = m_context.GetAvailablePhysicalDevices();
        if (allDevices.size() > 1) {
            ImGui::Separator();
            ImGui::Text("Detected GPUs in System (%zu):", allDevices.size());
            for (size_t i = 0; i < allDevices.size(); ++i) {
                ImGui::BulletText("[%zu] %s (%.2f GB VRAM)%s",
                                  i,
                                  allDevices[i].GetDeviceName().c_str(),
                                  allDevices[i].GetDedicatedVRAMGigabytes(),
                                  (allDevices[i].GetHandle() == activeGPU.GetHandle()) ? " [ACTIVE]" : "");
            }
        }
    }

    if (ImGui::CollapsingHeader("Swapchain & Dynamic Rendering State", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Extent: %d x %d", swapchain.GetExtent().width, swapchain.GetExtent().height);
        ImGui::Text("Color Format: VK_FORMAT_B8G8R8A8_UNORM");
        ImGui::Text("Depth Format: VK_FORMAT_D32_SFLOAT");
        ImGui::Text("Max Frames In Flight: %d", Swapchain::MAX_FRAMES_IN_FLIGHT);
        ImGui::Text("Rendering Pipeline: Vulkan 1.3 Dynamic Rendering (vkCmdBeginRendering)");

        ImGui::Separator();

        // Present Mode selector — changes take effect on next frame (safe swapchain recreate)
        const char* presentModes[] = {
            "Mailbox",
            "Immediate",
            "FIFO"
        };
        int currentMode = static_cast<int>(swapchain.GetPresentMode());
        ImGui::SetNextItemWidth(320.0f);
        if (ImGui::Combo("Present Mode", &currentMode, presentModes, IM_ARRAYSIZE(presentModes))) {
            swapchain.SetPresentMode(static_cast<Swapchain::PresentMode>(currentMode));
        }
        if (swapchain.HasPendingPresentModeChange()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "(applying...)");
        }
    }

    ImGui::End();
}

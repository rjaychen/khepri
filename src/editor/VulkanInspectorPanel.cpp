#include "VulkanInspectorPanel.h"
#include "../core/Logger.h"

VulkanInspectorPanel::VulkanInspectorPanel(VulkanContext& context)
    : m_context(context) {}

void VulkanInspectorPanel::RenderUI(const Swapchain& swapchain) {
    ImGui::Begin("Vulkan Educational Inspector");

    if (ImGui::CollapsingHeader("Physical Device & Driver", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto props = m_context.GetDeviceProperties();
        ImGui::Text("GPU Name: %s", props.deviceName);
        ImGui::Text("API Version: %d.%d.%d", VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion));
        ImGui::Text("Driver Version: %d", props.driverVersion);
        ImGui::Text("Vendor ID: 0x%X | Device ID: 0x%X", props.vendorID, props.deviceID);
    }

    if (ImGui::CollapsingHeader("Swapchain & Dynamic Rendering State", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Extent: %d x %d", swapchain.GetExtent().width, swapchain.GetExtent().height);
        ImGui::Text("Color Format: VK_FORMAT_B8G8R8A8_UNORM");
        ImGui::Text("Depth Format: VK_FORMAT_D32_SFLOAT");
        ImGui::Text("Max Frames In Flight: %d", Swapchain::MAX_FRAMES_IN_FLIGHT);
        ImGui::Text("Rendering Pipeline: Vulkan 1.3 Dynamic Rendering (vkCmdBeginRendering)");
    }

    if (ImGui::CollapsingHeader("Engine Logs", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Clear Logs")) Logger::Get().ClearLogs();
        ImGui::BeginChild("LogRegion", ImVec2(0, 200), true);
        for (const auto& log : Logger::Get().GetLogs()) {
            ImVec4 col = ImVec4(1, 1, 1, 1);
            if (log.level == LogLevel::Error) col = ImVec4(1, 0.3f, 0.3f, 1);
            else if (log.level == LogLevel::Warning) col = ImVec4(1, 0.8f, 0.2f, 1);
            else if (log.level == LogLevel::VulkanDebug) col = ImVec4(0.4f, 0.8f, 1.0f, 1);

            ImGui::TextColored(col, "[%s] %s", log.timestamp.c_str(), log.message.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

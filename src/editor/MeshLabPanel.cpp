#include "MeshLabPanel.h"
#include "../mesh/CDT.h"
#include "../core/Logger.h"

MeshLabPanel::MeshLabPanel(VulkanContext& context)
    : m_context(context) {}

void MeshLabPanel::RenderUI(std::shared_ptr<MeshComponent>& activeDisplayMesh) {
    ImGui::Begin("Mesh Generation Workbench");

    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Khepri Mesh Studio & Topology Lab");
    ImGui::Separator();

    // Section 1: Mesh Primitive Generator & Topology Stats
    if (ImGui::CollapsingHeader("1. Mesh Primitive Generator & Topology Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Combo("Primitive", &m_primitiveType, "Cube\0Sphere\0Cylinder\0Plane\0");
        if (ImGui::Button("Generate Primitive Mesh")) {
            if (m_primitiveType == 0) activeDisplayMesh = MeshComponent::CreateCube(m_context, 2.0f);
            else if (m_primitiveType == 1) activeDisplayMesh = MeshComponent::CreateSphere(m_context, 1.2f, 32, 16);
            else if (m_primitiveType == 2) activeDisplayMesh = MeshComponent::CreateCylinder(m_context, 0.8f, 2.0f, 32);
            else activeDisplayMesh = MeshComponent::CreatePlane(m_context, 4.0f, 8);

            if (activeDisplayMesh) {
                m_authoringMesh.BuildFromIndexedMesh(activeDisplayMesh->GetVertices(), activeDisplayMesh->GetIndices());
            }
        }

        if (activeDisplayMesh) {
            ImGui::Separator();
            ImGui::Text("Active Mesh Statistics:");
            ImGui::BulletText("Vertex Count: %zu", activeDisplayMesh->GetVertices().size());
            ImGui::BulletText("Triangle Count: %zu", activeDisplayMesh->GetIndices().size() / 3);

            if (m_authoringMesh.GetVertices().size() > 0) {
                ImGui::BulletText("Half-Edges: %zu", m_authoringMesh.GetHalfEdges().size());
                ImGui::BulletText("Euler Characteristic (V-E+F): %u", m_authoringMesh.GetEulerCharacteristic());
            }
        }
    }

    // Section 2: Data-Oriented Mesh Baking
    if (ImGui::CollapsingHeader("2. Data-Oriented Mesh Baking", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("Flattens index-based HalfEdgeMesh topology into Vulkan Render Mesh buffers.");
        if (ImGui::Button("Bake Authoring Mesh to Render Buffers")) {
            if (m_authoringMesh.GetVertices().empty() && activeDisplayMesh) {
                m_authoringMesh.BuildFromIndexedMesh(activeDisplayMesh->GetVertices(), activeDisplayMesh->GetIndices());
            }
            if (!m_authoringMesh.GetVertices().empty()) {
                std::vector<Vertex> bakedVertices;
                std::vector<uint32_t> bakedIndices;
                m_authoringMesh.BakeToRenderMesh(bakedVertices, bakedIndices);
                activeDisplayMesh = std::make_shared<MeshComponent>(m_context, bakedVertices, bakedIndices);
                LOG_INFO("Successfully baked HalfEdgeMesh topology to GPU Render Mesh (" +
                         std::to_string(bakedVertices.size()) + " vertices, " +
                         std::to_string(bakedIndices.size() / 3) + " triangles)");
            }
        }
    }

    ImGui::End();
}

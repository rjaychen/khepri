#include "MeshLabPanel.h"
#include "../mesh/CDT.h"
#include "../mesh/MeshBoolean.h"
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

    // Section 2: CSG Mesh Booleans
    if (ImGui::CollapsingHeader("2. Constructive Solid Geometry (CSG Booleans)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("Performs CSG operations between Mesh A (Cube) and Mesh B (Sphere).");
        ImGui::RadioButton("Union (A ∪ B)", &m_booleanOp, 0); ImGui::SameLine();
        ImGui::RadioButton("Intersection (A ∩ B)", &m_booleanOp, 1); ImGui::SameLine();
        ImGui::RadioButton("Difference (A \\ B)", &m_booleanOp, 2);

        if (ImGui::Button("Execute CSG Boolean Operation")) {
            auto cube = MeshComponent::CreateCube(m_context, 2.0f);
            auto sphere = MeshComponent::CreateSphere(m_context, 1.2f, 32, 16);
            BooleanOp op = (m_booleanOp == 0) ? BooleanOp::Union :
                           (m_booleanOp == 1) ? BooleanOp::Intersection : BooleanOp::Difference;
            activeDisplayMesh = MeshBoolean::PerformBoolean(m_context, *cube, *sphere, op);
            if (activeDisplayMesh) {
                m_authoringMesh.BuildFromIndexedMesh(activeDisplayMesh->GetVertices(), activeDisplayMesh->GetIndices());
            }
        }
    }

    ImGui::End();
}

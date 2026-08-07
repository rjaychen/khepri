#include "MeshLabPanel.h"
#include "../mesh/CDT.h"
#include "../mesh/MeshBoolean.h"
#include "../mesh/LDNI.h"
#include "../core/Logger.h"

MeshLabPanel::MeshLabPanel(VulkanContext& context)
    : m_context(context) {}

void MeshLabPanel::RenderUI(std::shared_ptr<MeshComponent>& activeDisplayMesh) {
    ImGui::Begin("Mesh Generation Workbench");

    ImGui::Text("Computational Geometry Operations");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("1. Constrained Delaunay Triangulation (CDT)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("Generates Delaunay triangulation from a 3D planar constraint polygon.");
        if (ImGui::Button("Generate CDT Polygon Mesh")) {
            std::vector<glm::vec3> poly = {
                {-2.0f, 0.0f, -2.0f},
                { 2.0f, 0.0f, -2.0f},
                { 3.0f, 0.0f,  0.0f},
                { 1.0f, 0.0f,  2.0f},
                {-2.0f, 0.0f,  1.0f}
            };
            activeDisplayMesh = CDT::Triangulate3DPolygon(m_context, poly);
            LOG_INFO("Created CDT Triangulated Surface Mesh");
        }
    }

    if (ImGui::CollapsingHeader("2. CSG Mesh Booleans", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::RadioButton("Union", &m_booleanOp, 0); ImGui::SameLine();
        ImGui::RadioButton("Intersection", &m_booleanOp, 1); ImGui::SameLine();
        ImGui::RadioButton("Difference", &m_booleanOp, 2);

        if (ImGui::Button("Execute CSG Boolean (Cube & Sphere)")) {
            auto cube = MeshComponent::CreateCube(m_context, 2.0f);
            auto sphere = MeshComponent::CreateSphere(m_context, 1.2f, 32, 16);
            BooleanOp op = (m_booleanOp == 0) ? BooleanOp::Union :
                           (m_booleanOp == 1) ? BooleanOp::Intersection : BooleanOp::Difference;
            activeDisplayMesh = MeshBoolean::PerformBoolean(m_context, *cube, *sphere, op);
        }
    }

    if (ImGui::CollapsingHeader("3. Layered Depth-Normal Images (LDNI)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Lattice Resolution", &m_ldniRes, 16, 256);
        if (ImGui::Button("Execute LDNI Ray-Interval Contouring")) {
            auto sphere = MeshComponent::CreateSphere(m_context, 1.5f, 32, 16);
            LDNI ldni(m_ldniRes, m_ldniRes);
            ldni.GenerateFromMesh(*sphere);
            activeDisplayMesh = ldni.ExtractContouredMesh(m_context);
        }
    }

    ImGui::End();
}

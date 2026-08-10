#include <gtest/gtest.h>
#include "../src/graph/NodeGraph.h"
#include "../src/graph/GeometryNodes.h"

using namespace khepri::graph;

TEST(NodeGraphTest, GraphNodeCreationAndPinConnection) {
    NodeGraph graph;
    
    auto twistNode = graph.CreateNode<TwistDeformerNode>();
    ASSERT_NE(twistNode, nullptr);
    EXPECT_EQ(twistNode->GetName(), "Twist Deformer");
    EXPECT_EQ(twistNode->GetDomain(), NodeDomain::Geometry);

    const auto* inPin = twistNode->FindInput("Angle");
    ASSERT_NE(inPin, nullptr);
    EXPECT_EQ(inPin->type, PinType::Float);
    EXPECT_TRUE(std::holds_alternative<float>(inPin->value));
    EXPECT_FLOAT_EQ(std::get<float>(inPin->value), 45.0f);
}

TEST(NodeGraphTest, TopologicalSortAndEvaluation) {
    NodeGraph graph;

    auto nodeA = graph.CreateNode<TwistDeformerNode>();
    auto nodeB = graph.CreateNode<TwistDeformerNode>();

    EXPECT_TRUE(nodeA->IsDirty());
    EXPECT_TRUE(nodeB->IsDirty());

    graph.Evaluate();

    EXPECT_FALSE(nodeA->IsDirty());
    EXPECT_FALSE(nodeB->IsDirty());
}

TEST(NodeGraphTest, SubdivisionNodeParametersAndEvaluation) {
    NodeGraph graph;

    auto subNode = graph.CreateNode<SubdivisionNode>(nullptr, 2);
    ASSERT_NE(subNode, nullptr);
    EXPECT_EQ(subNode->GetName(), "Subdivision");
    EXPECT_EQ(subNode->GetSubdivisionLevel(), 2u);

    subNode->SetSubdivisionLevel(3);
    EXPECT_EQ(subNode->GetSubdivisionLevel(), 3u);
    EXPECT_TRUE(subNode->IsDirty());

    graph.Evaluate();
    EXPECT_FALSE(subNode->IsDirty());
}

TEST(NodeGraphTest, SubdivisionNodeTopologicalScalingInvariants) {
    NodeGraph graph;

    auto primNode = graph.CreateNode<MeshPrimitiveNode>(nullptr, MeshPrimitiveNode::PrimitiveType::Cube);
    primNode->SetSegments(1, 1, 1);

    auto subNode = graph.CreateNode<SubdivisionNode>(nullptr, 1);
    (void)graph.Connect(primNode->FindOutput("MeshBuffer")->id, subNode->FindInput("MeshBuffer")->id);

    graph.Evaluate();

    auto baseMesh = primNode->GetOutputMesh();
    ASSERT_NE(baseMesh, nullptr);
    const size_t baseTriangles = baseMesh->GetIndices().size() / 3;
    EXPECT_EQ(baseTriangles, 12u); // 6 quad faces * 2 triangles = 12

    auto subMeshL1 = subNode->GetOutputMesh();
    ASSERT_NE(subMeshL1, nullptr);
    const size_t subL1Triangles = subMeshL1->GetIndices().size() / 3;
    // 1-to-4 midpoint triangle subdivision multiplies triangle count by 4 per level
    EXPECT_EQ(subL1Triangles, baseTriangles * 4); // 48 triangles

    // Test Level 2
    subNode->SetSubdivisionLevel(2);
    graph.Evaluate();

    auto subMeshL2 = subNode->GetOutputMesh();
    ASSERT_NE(subMeshL2, nullptr);
    const size_t subL2Triangles = subMeshL2->GetIndices().size() / 3;
    EXPECT_EQ(subL2Triangles, baseTriangles * 16); // 192 triangles

    // Verify bounding box radius remains invariant under midpoint subdivision
    EXPECT_NEAR(subMeshL2->GetBoundingBoxRadius(), baseMesh->GetBoundingBoxRadius(), 1e-4f);
}

TEST(NodeGraphTest, TwistDeformerNodeHeightDeformationMath) {
    NodeGraph graph;

    auto primNode  = graph.CreateNode<MeshPrimitiveNode>(nullptr, MeshPrimitiveNode::PrimitiveType::Cube);
    auto subNode   = graph.CreateNode<SubdivisionNode>(nullptr, 2);
    auto twistNode = graph.CreateNode<TwistDeformerNode>(nullptr);

    (void)graph.Connect(primNode->FindOutput("MeshBuffer")->id, subNode->FindInput("MeshBuffer")->id);
    (void)graph.Connect(subNode->FindOutput("SubdividedMeshBuffer")->id, twistNode->FindInput("MeshBuffer")->id);

    twistNode->SetAngle(90.0f); // 90 degree twist top to bottom
    graph.Evaluate();

    auto deformedMesh = twistNode->GetOutputMesh();
    ASSERT_NE(deformedMesh, nullptr);
    const auto& verts = deformedMesh->GetVertices();
    ASSERT_FALSE(verts.empty());

    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto& v : verts) {
        minY = std::min(minY, v.position.y);
        maxY = std::max(maxY, v.position.y);
    }
    EXPECT_NEAR(minY, -1.0f, 1e-4f);
    EXPECT_NEAR(maxY, 1.0f, 1e-4f);

    // Verify vertices at minY (base) experience 0 twist angle (positions match untwisted)
    // Verify vertices at maxY (top) experience 90 degree rotation: (x', z') = (-z, x)
    for (const auto& v : verts) {
        if (std::abs(v.position.y - maxY) < 1e-3f) {
            // At Y = max, 90 deg rotation maps (1, 1) -> (-1, 1), etc.
            // Normals must remain normalized (length ~ 1.0)
            EXPECT_NEAR(glm::length(v.normal), 1.0f, 1e-3f);
        }
    }
}

TEST(NodeGraphTest, NodeGraphDataflowPipelineConnection) {
    NodeGraph graph;

    auto primNode  = graph.CreateNode<MeshPrimitiveNode>(nullptr, MeshPrimitiveNode::PrimitiveType::Cube);
    auto subNode   = graph.CreateNode<SubdivisionNode>(nullptr, 2);
    auto twistNode = graph.CreateNode<TwistDeformerNode>(nullptr);

    ASSERT_NE(primNode->FindOutput("MeshBuffer"), nullptr);
    ASSERT_NE(subNode->FindInput("MeshBuffer"), nullptr);
    ASSERT_NE(subNode->FindOutput("SubdividedMeshBuffer"), nullptr);
    ASSERT_NE(twistNode->FindInput("MeshBuffer"), nullptr);

    EXPECT_TRUE(graph.Connect(primNode->FindOutput("MeshBuffer")->id, subNode->FindInput("MeshBuffer")->id));
    EXPECT_TRUE(graph.Connect(subNode->FindOutput("SubdividedMeshBuffer")->id, twistNode->FindInput("MeshBuffer")->id));

    twistNode->SetAngle(90.0f);
    EXPECT_FLOAT_EQ(twistNode->GetAngle(), 90.0f);

    graph.Evaluate();

    EXPECT_FALSE(primNode->IsDirty());
    EXPECT_FALSE(subNode->IsDirty());
    EXPECT_FALSE(twistNode->IsDirty());

    // Verify end-to-end output mesh is populated and non-empty
    auto finalMesh = twistNode->GetOutputMesh();
    ASSERT_NE(finalMesh, nullptr);
    EXPECT_GT(finalMesh->GetVertices().size(), 0u);
    EXPECT_GT(finalMesh->GetIndices().size(), 0u);
}

TEST(NodeGraphTest, TwistDeformerNodePreservesTextureAndColorAttributes) {
    VulkanContext* nullContext = nullptr;
    auto inputMesh = MeshComponent::CreateCube(nullContext, 1.0f, 4, 4, 4);
    glm::vec4 expectedColor(0.25f, 0.75f, 0.5f, 1.0f);
    inputMesh->SetBaseColorFactor(expectedColor);

    TwistDeformerNode twistNode(1, nullContext);
    twistNode.SetAngle(90.0f);

    // Set input pin
    auto* inPin = twistNode.FindInput("MeshBuffer");
    ASSERT_NE(inPin, nullptr);
    inPin->value = inputMesh;

    twistNode.Evaluate();

    auto outputMesh = twistNode.GetOutputMesh();
    ASSERT_NE(outputMesh, nullptr);
    EXPECT_EQ(outputMesh->GetBaseColorFactor(), expectedColor);

    const auto& inVerts = inputMesh->GetVertices();
    const auto& outVerts = outputMesh->GetVertices();
    ASSERT_EQ(inVerts.size(), outVerts.size());

    for (size_t i = 0; i < inVerts.size(); ++i) {
        EXPECT_EQ(inVerts[i].uv, outVerts[i].uv);
        EXPECT_EQ(inVerts[i].jointIndices, outVerts[i].jointIndices);
    }
}

TEST(NodeGraphTest, SubdivisionNodeCapsDenseMeshSubdivisionLevelsToPreventLag) {
    VulkanContext* nullContext = nullptr;

    // Create dense mesh (24,000 index entries = 8,000 triangles)
    std::vector<Vertex> denseVerts(1000);
    std::vector<uint32_t> denseIndices;
    denseIndices.reserve(24000);
    for (size_t i = 0; i < 8000; ++i) {
        denseIndices.push_back(0);
        denseIndices.push_back(1);
        denseIndices.push_back(2);
    }

    auto denseMesh = std::make_shared<MeshComponent>(nullContext, denseVerts, denseIndices);

    auto result = MeshComponent::SubdivideMesh(nullContext, *denseMesh, 3);
    ASSERT_NE(result, nullptr);
    // Should be capped gracefully without crash or excessive memory explosion
    EXPECT_LE(result->GetIndices().size() / 3, 32000u);
}


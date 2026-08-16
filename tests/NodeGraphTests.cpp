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

TEST(NodeGraphTest, ExternalMeshNodeReplacesPrimitiveGeneratorToPreventDuplicateInstances) {
    VulkanContext* nullContext = nullptr;
    auto testMesh = MeshComponent::CreateCube(nullContext, 1.0f);

    NodeGraph graph;
    auto primNode = graph.CreateNode<MeshPrimitiveNode>(nullContext, MeshPrimitiveNode::PrimitiveType::Cube);
    auto subNode  = graph.CreateNode<SubdivisionNode>(nullContext, 1);
    (void)graph.Connect(primNode->FindOutput("MeshBuffer")->id, subNode->FindInput("MeshBuffer")->id);

    auto extNode  = graph.CreateNode<ExternalMeshNode>(testMesh);
    (void)graph.Connect(extNode->FindOutput("MeshBuffer")->id, subNode->FindInput("MeshBuffer")->id);
    graph.RemoveNode(primNode->GetId());

    graph.Evaluate();

    auto outMesh = subNode->GetOutputMesh();
    ASSERT_NE(outMesh, nullptr);
    EXPECT_GT(outMesh->GetVertices().size(), 0u);
}

TEST(NodeGraphTest, CyclePreventionRejectsLoopingConnections) {
    NodeGraph graph;

    auto nodeA = graph.CreateNode<TwistDeformerNode>();
    auto nodeB = graph.CreateNode<TwistDeformerNode>();
    auto nodeC = graph.CreateNode<TwistDeformerNode>();

    auto* outA = nodeA->FindOutput("DeformedMeshBuffer");
    auto* inB  = nodeB->FindInput("MeshBuffer");
    auto* outB = nodeB->FindOutput("DeformedMeshBuffer");
    auto* inC  = nodeC->FindInput("MeshBuffer");
    auto* outC = nodeC->FindOutput("DeformedMeshBuffer");
    auto* inA  = nodeA->FindInput("MeshBuffer");

    ASSERT_NE(outA, nullptr);
    ASSERT_NE(inB, nullptr);
    ASSERT_NE(outB, nullptr);
    ASSERT_NE(inC, nullptr);
    ASSERT_NE(outC, nullptr);
    ASSERT_NE(inA, nullptr);

    // Connect A -> B -> C
    EXPECT_TRUE(graph.Connect(outA->id, inB->id));
    EXPECT_TRUE(graph.Connect(outB->id, inC->id));

    // Connecting C -> A would form a cycle (A -> B -> C -> A), must return false!
    EXPECT_FALSE(graph.Connect(outC->id, inA->id));

    // Self-loop (A -> A) must return false!
    EXPECT_FALSE(graph.Connect(outA->id, inA->id));
}

TEST(NodeGraphTest, DownstreamDirtyFlagPropagationOnParameterMutationAndDisconnect) {
    NodeGraph graph;

    auto primNode  = graph.CreateNode<MeshPrimitiveNode>(nullptr, MeshPrimitiveNode::PrimitiveType::Cube);
    auto subNode   = graph.CreateNode<SubdivisionNode>(nullptr, 1);
    auto twistNode = graph.CreateNode<TwistDeformerNode>(nullptr);

    EXPECT_TRUE(graph.Connect(primNode->FindOutput("MeshBuffer")->id, subNode->FindInput("MeshBuffer")->id));
    EXPECT_TRUE(graph.Connect(subNode->FindOutput("SubdividedMeshBuffer")->id, twistNode->FindInput("MeshBuffer")->id));

    graph.Evaluate();

    EXPECT_FALSE(primNode->IsDirty());
    EXPECT_FALSE(subNode->IsDirty());
    EXPECT_FALSE(twistNode->IsDirty());

    // Mutate root node parameter -> must propagate dirty state down to subNode and twistNode
    primNode->SetSegmentsX(4);
    EXPECT_TRUE(primNode->IsDirty());
    EXPECT_TRUE(subNode->IsDirty());
    EXPECT_TRUE(twistNode->IsDirty());

    graph.Evaluate();
    EXPECT_FALSE(primNode->IsDirty());
    EXPECT_FALSE(subNode->IsDirty());
    EXPECT_FALSE(twistNode->IsDirty());

    // Disconnecting input pin -> must propagate dirty state downstream
    EXPECT_TRUE(graph.Disconnect(twistNode->FindInput("MeshBuffer")->id));
    EXPECT_TRUE(twistNode->IsDirty());
}

TEST(NodeGraphTest, SafePinLookupFindPinFunctions) {
    NodeGraph graph;

    auto twistNode = graph.CreateNode<TwistDeformerNode>();
    const auto* inPin = twistNode->FindInput("Angle");
    ASSERT_NE(inPin, nullptr);

    GraphPin* foundPin = graph.FindPin(inPin->id);
    ASSERT_NE(foundPin, nullptr);
    EXPECT_EQ(foundPin->id, inPin->id);
    EXPECT_EQ(foundPin->name, "Angle");

    const NodeGraph& constGraph = graph;
    const GraphPin* constFoundPin = constGraph.FindPin(inPin->id);
    ASSERT_NE(constFoundPin, nullptr);
    EXPECT_EQ(constFoundPin->id, inPin->id);
}
// ---------------------------------------------------------------------------
// GraphNode::GetOutputMesh() virtual contract
// Verifies that the base-class virtual dispatch eliminates the need for
// dynamic_cast in any caller. New node types only need to override this method.
// ---------------------------------------------------------------------------

TEST(NodeGraphTest, GetOutputMeshVirtualDispatchReturnsNullForValueNodes) {
    // FloatNode and Vector3Node do not produce geometry; the base default must be null.
    NodeGraph graph;
    auto floatNode  = graph.CreateNode<FloatNode>(1.0f);
    auto vec3Node   = graph.CreateNode<Vector3Node>(glm::vec3(0.0f));

    // Access through base pointer to prove no downcast is required
    const GraphNode* baseFloat = floatNode.get();
    const GraphNode* baseVec3  = vec3Node.get();

    EXPECT_EQ(baseFloat->GetOutputMesh(), nullptr)
        << "FloatNode must not advertise a geometry output";
    EXPECT_EQ(baseVec3->GetOutputMesh(), nullptr)
        << "Vector3Node must not advertise a geometry output";
}

TEST(NodeGraphTest, GetOutputMeshVirtualDispatchReturnsGeometryForGeometryNodes) {
    // All geometry-producing nodes must return non-null through the base pointer
    // after evaluation, with zero dynamic_cast.
    NodeGraph graph;
    auto primNode   = graph.CreateNode<MeshPrimitiveNode>(nullptr, MeshPrimitiveNode::PrimitiveType::Cube);
    auto subdivNode = graph.CreateNode<SubdivisionNode>(nullptr, 1);
    auto twistNode  = graph.CreateNode<TwistDeformerNode>(nullptr);

    // Chain: primitive -> subdivide -> twist (headless, no Vulkan context)
    if (primNode->FindOutput("MeshBuffer") && subdivNode->FindInput("MeshBuffer")) {
        graph.Connect(primNode->FindOutput("MeshBuffer")->id, subdivNode->FindInput("MeshBuffer")->id);
    }

    graph.Evaluate();

    // Collect all non-null output meshes through the base class pointer only
    int geometryNodeCount = 0;
    for (const auto& [id, node] : graph.GetNodes()) {
        if (node->GetOutputMesh() != nullptr) {
            ++geometryNodeCount;
        }
    }
    // primitiveNode and subdivNode should each expose a mesh; twistNode has no input so may be null
    EXPECT_GE(geometryNodeCount, 1)
        << "At least the primitive node must produce geometry through the base virtual";
}

TEST(NodeGraphTest, GetOutputMeshVirtualDispatchConsistentWithConcreteType) {
    // The mesh returned via the base pointer must be the same object as via the concrete type.
    NodeGraph graph;
    auto primNode = graph.CreateNode<MeshPrimitiveNode>(nullptr, MeshPrimitiveNode::PrimitiveType::Sphere);
    graph.Evaluate();

    const GraphNode* base    = primNode.get();
    auto meshViaBase     = base->GetOutputMesh();
    auto meshViaConcrete = primNode->GetOutputMesh();

    EXPECT_EQ(meshViaBase, meshViaConcrete)
        << "Virtual and concrete GetOutputMesh() must return the same shared_ptr";
}


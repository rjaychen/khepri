#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../src/graph/NodeGraph.h"
#include "../src/graph/GeometryNodes.h"
#include "../src/scene/SceneNode.h"
#include "../src/scene/LightComponent.h"
#include "../src/editor/SceneTreePanel.h"
#include "../src/mesh/ModelImporter.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

using namespace khepri::graph;

// ---------------------------------------------------------------------------
// 1. Node Graph Multi-Node & CSG Dataflow Oracles
// ---------------------------------------------------------------------------

TEST(WorkflowAndLifecycleTest, CSGBooleanMultiInputDataflow) {
    NodeGraph graph;
    VulkanContext* nullContext = nullptr;

    // Create 2 source geometry nodes
    Vertex vA0{}; vA0.position = {-1.0f, -1.0f, 0.0f}; vA0.normal = {0.0f, 0.0f, 1.0f}; vA0.uv = {0.0f, 0.0f};
    Vertex vA1{}; vA1.position = { 1.0f, -1.0f, 0.0f}; vA1.normal = {0.0f, 0.0f, 1.0f}; vA1.uv = {1.0f, 0.0f};
    Vertex vA2{}; vA2.position = { 0.0f,  1.0f, 0.0f}; vA2.normal = {0.0f, 0.0f, 1.0f}; vA2.uv = {0.5f, 1.0f};
    std::vector<Vertex> vertsA = {vA0, vA1, vA2};
    std::vector<uint32_t> indicesA = {0, 1, 2};
    auto meshA = std::make_shared<MeshComponent>(nullContext, vertsA, indicesA);

    Vertex vB0{}; vB0.position = {-0.5f, -0.5f, 0.5f}; vB0.normal = {0.0f, 0.0f, 1.0f}; vB0.uv = {0.0f, 0.0f};
    Vertex vB1{}; vB1.position = { 0.5f, -0.5f, 0.5f}; vB1.normal = {0.0f, 0.0f, 1.0f}; vB1.uv = {1.0f, 0.0f};
    Vertex vB2{}; vB2.position = { 0.0f,  0.5f, 0.5f}; vB2.normal = {0.0f, 0.0f, 1.0f}; vB2.uv = {0.5f, 1.0f};
    std::vector<Vertex> vertsB = {vB0, vB1, vB2};
    std::vector<uint32_t> indicesB = {0, 1, 2};
    auto meshB = std::make_shared<MeshComponent>(nullContext, vertsB, indicesB);

    auto nodeA = graph.CreateNode<ExternalMeshNode>(meshA);
    auto nodeB = graph.CreateNode<ExternalMeshNode>(meshB);
    auto csgNode = graph.CreateNode<CSGBooleanNode>(nullContext, CSGBooleanNode::OpType::Union);

    ASSERT_NE(nodeA, nullptr);
    ASSERT_NE(nodeB, nullptr);
    ASSERT_NE(csgNode, nullptr);

    // Connect nodeA -> csgNode (Mesh A) and nodeB -> csgNode (Mesh B)
    const auto* outPinA = nodeA->FindOutput("MeshBuffer");
    const auto* outPinB = nodeB->FindOutput("MeshBuffer");
    const auto* inPinA = csgNode->FindInput("Mesh A");
    const auto* inPinB = csgNode->FindInput("Mesh B");

    ASSERT_NE(outPinA, nullptr);
    ASSERT_NE(outPinB, nullptr);
    ASSERT_NE(inPinA, nullptr);
    ASSERT_NE(inPinB, nullptr);

    EXPECT_TRUE(graph.Connect(outPinA->id, inPinA->id));
    EXPECT_TRUE(graph.Connect(outPinB->id, inPinB->id));

    // Evaluate entire graph
    graph.Evaluate();

    // Verify csgNode received input values and assigned its result output pin
    const auto* resultPin = csgNode->FindOutput("ResultMesh");
    ASSERT_NE(resultPin, nullptr);
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<MeshComponent>>(resultPin->value));
    auto outputMesh = std::get<std::shared_ptr<MeshComponent>>(resultPin->value);
    EXPECT_NE(outputMesh, nullptr);
}

TEST(WorkflowAndLifecycleTest, CSGBooleanDisconnectedInputFallback) {
    NodeGraph graph;
    VulkanContext* nullContext = nullptr;

    // csgNode with disconnected inputs in headless mode
    auto csgNode = graph.CreateNode<CSGBooleanNode>(nullContext, CSGBooleanNode::OpType::Intersection);
    ASSERT_NE(csgNode, nullptr);

    // Evaluate without throwing or crashing
    EXPECT_NO_THROW(graph.Evaluate());
    EXPECT_FALSE(csgNode->IsDirty());
}

TEST(WorkflowAndLifecycleTest, CSGBooleanOpTypeMutationMarksDirty) {
    NodeGraph graph;
    auto csgNode = graph.CreateNode<CSGBooleanNode>(nullptr, CSGBooleanNode::OpType::Union);
    graph.Evaluate();
    EXPECT_FALSE(csgNode->IsDirty());

    csgNode->SetOpType(CSGBooleanNode::OpType::Difference);
    EXPECT_EQ(csgNode->GetOpType(), CSGBooleanNode::OpType::Difference);
    EXPECT_TRUE(csgNode->IsDirty());
}

TEST(WorkflowAndLifecycleTest, ParallelBranchTopologicalOrdering) {
    NodeGraph graph;

    // Create 2 independent parallel branches (Branch 1: Prim1 -> Subdiv, Branch 2: Prim2)
    auto prim1 = graph.CreateNode<MeshPrimitiveNode>(nullptr, MeshPrimitiveNode::PrimitiveType::Cube);
    auto sub1  = graph.CreateNode<SubdivisionNode>(nullptr, 1);
    auto prim2 = graph.CreateNode<MeshPrimitiveNode>(nullptr, MeshPrimitiveNode::PrimitiveType::Sphere);

    EXPECT_TRUE(graph.Connect(prim1->FindOutput("MeshBuffer")->id, sub1->FindInput("MeshBuffer")->id));

    auto sorted = graph.TopologicalSort();
    ASSERT_EQ(sorted.size(), 3u);

    // In topological sort, prim1 must appear before sub1
    auto itPrim1 = std::find_if(sorted.begin(), sorted.end(), [&](const auto& n){ return n->GetId() == prim1->GetId(); });
    auto itSub1  = std::find_if(sorted.begin(), sorted.end(), [&](const auto& n){ return n->GetId() == sub1->GetId(); });
    EXPECT_LT(std::distance(sorted.begin(), itPrim1), std::distance(sorted.begin(), itSub1));
}

// ---------------------------------------------------------------------------
// 2. Scene Tree Hierarchy & Pointer Safety Oracles
// ---------------------------------------------------------------------------

TEST(WorkflowAndLifecycleTest, SceneNodeHierarchyContainsCheck) {
    auto root = std::make_unique<SceneNode>("Root");
    auto child1 = std::make_unique<SceneNode>("Child 1");
    auto child2 = std::make_unique<SceneNode>("Child 2");
    auto grandChild = std::make_unique<SceneNode>("GrandChild");

    SceneNode* child1Ptr = root->AddChild(std::move(child1));
    SceneNode* child2Ptr = root->AddChild(std::move(child2));
    SceneNode* grandChildPtr = child1Ptr->AddChild(std::move(grandChild));

    auto foreignNode = std::make_unique<SceneNode>("Foreign");

    // Self check
    EXPECT_TRUE(root->Contains(root.get()));
    // Direct children
    EXPECT_TRUE(root->Contains(child1Ptr));
    EXPECT_TRUE(root->Contains(child2Ptr));
    // Nested grandchild
    EXPECT_TRUE(root->Contains(grandChildPtr));
    EXPECT_TRUE(child1Ptr->Contains(grandChildPtr));
    EXPECT_FALSE(child2Ptr->Contains(grandChildPtr));
    // Foreign / Unrelated node
    EXPECT_FALSE(root->Contains(foreignNode.get()));
    EXPECT_FALSE(root->Contains(nullptr));
}

TEST(WorkflowAndLifecycleTest, SelectionPointerValidationOnSceneReload) {
    VulkanContext* nullContext = nullptr;
    SceneTreePanel panel(nullContext);

    auto foreignRoot = std::make_unique<SceneNode>("Foreign Root");
    auto foreignChild = std::make_unique<SceneNode>("Foreign Child");
    SceneNode* foreignChildPtr = foreignRoot->AddChild(std::move(foreignChild));

    panel.SetSelectedNode(foreignChildPtr);
    EXPECT_EQ(panel.GetSelectedNode(), foreignChildPtr);

    auto activeRoot = std::make_unique<SceneNode>("Active Root");
    auto activeChild = std::make_unique<SceneNode>("Active Child");
    SceneNode* activeChildPtr = activeRoot->AddChild(std::move(activeChild));

    // foreignChildPtr is not in activeRoot, so ValidateSelection must auto-clear it
    panel.ValidateSelection(activeRoot.get());
    EXPECT_EQ(panel.GetSelectedNode(), nullptr);

    // Selecting a node that exists in activeRoot passes validation
    panel.SetSelectedNode(activeChildPtr);
    panel.ValidateSelection(activeRoot.get());
    EXPECT_EQ(panel.GetSelectedNode(), activeChildPtr);

    // Validating against null root clears selection
    panel.ValidateSelection(nullptr);
    EXPECT_EQ(panel.GetSelectedNode(), nullptr);
}

TEST(WorkflowAndLifecycleTest, SubtreeDeletionSelectionAutoInvalidation) {
    VulkanContext* nullContext = nullptr;
    SceneTreePanel panel(nullContext);

    auto root = std::make_unique<SceneNode>("Root");
    auto branch = std::make_unique<SceneNode>("Branch");
    auto leaf = std::make_unique<SceneNode>("Leaf");

    SceneNode* branchPtr = root->AddChild(std::move(branch));
    SceneNode* leafPtr = branchPtr->AddChild(std::move(leaf));

    // Select the deep leaf node
    panel.SetSelectedNode(leafPtr);
    EXPECT_EQ(panel.GetSelectedNode(), leafPtr);

    // If branch is deleted, check if branch->Contains(leafPtr) correctly identifies the leaf
    EXPECT_TRUE(branchPtr->Contains(panel.GetSelectedNode()));

    // Delete the branch from root
    root->RemoveChild(branchPtr);

    // ValidateSelection cleans up dangling selection
    panel.ValidateSelection(root.get());
    EXPECT_EQ(panel.GetSelectedNode(), nullptr);
}

// ---------------------------------------------------------------------------
// 3. Light Gizmo Affine Transform Invariant
// ---------------------------------------------------------------------------

TEST(WorkflowAndLifecycleTest, LightGizmoScaleIndependence) {
    // When a light node has position, rotation, and arbitrary scaling (e.g. 10x)
    SceneNode lightNode("Light");
    lightNode.position = glm::vec3(3.0f, 5.0f, -2.0f);
    lightNode.rotationDegrees = glm::vec3(45.0f, 30.0f, 0.0f);
    lightNode.scale = glm::vec3(10.0f, 10.0f, 10.0f); // Large scale applied

    glm::mat4 worldTransform = lightNode.GetWorldTransform();

    // Extract unscaled affine matrix as done in DrawSceneNode
    glm::vec3 worldPos = glm::vec3(worldTransform[3]);
    glm::mat3 rotMat(worldTransform);
    if (glm::length(rotMat[0]) > 1e-5f) rotMat[0] = glm::normalize(rotMat[0]);
    if (glm::length(rotMat[1]) > 1e-5f) rotMat[1] = glm::normalize(rotMat[1]);
    if (glm::length(rotMat[2]) > 1e-5f) rotMat[2] = glm::normalize(rotMat[2]);

    glm::mat4 unscaledTransform = glm::mat4(rotMat);
    unscaledTransform[3] = glm::vec4(worldPos, 1.0f);

    // Verify position is preserved exactly
    EXPECT_FLOAT_EQ(unscaledTransform[3].x, 3.0f);
    EXPECT_FLOAT_EQ(unscaledTransform[3].y, 5.0f);
    EXPECT_FLOAT_EQ(unscaledTransform[3].z, -2.0f);

    // Verify column vectors have unit length 1.0 (scale stripped)
    EXPECT_NEAR(glm::length(glm::vec3(unscaledTransform[0])), 1.0f, 1e-5f);
    EXPECT_NEAR(glm::length(glm::vec3(unscaledTransform[1])), 1.0f, 1e-5f);
    EXPECT_NEAR(glm::length(glm::vec3(unscaledTransform[2])), 1.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// 4. Model Importer Error Resilience
// ---------------------------------------------------------------------------

TEST(WorkflowAndLifecycleTest, ModelImporterNonExistentFileReturnsNullSafely) {
    VulkanContext* nullContext = nullptr;
    auto node1 = ModelImporter::LoadFromFile(*nullContext, "non_existent_path.gltf");
    EXPECT_EQ(node1, nullptr);

    auto node2 = ModelImporter::LoadFromFile(*nullContext, "assets/corrupted_file.obj");
    EXPECT_EQ(node2, nullptr);

    auto node3 = ModelImporter::LoadFromFile(*nullContext, "invalid_extension.xyz");
    EXPECT_EQ(node3, nullptr);
}

#include <gtest/gtest.h>
#include "editor/NodeGraphEditorPanel.h"
#include "graph/GeometryNodes.h"

using namespace khepri;
using namespace khepri::graph;

TEST(NodeGraphEditorUITest, PinTypeColorPaletteMatchesSpecification) {
    EXPECT_EQ(NodeGraphEditorPanel::GetPinColor(PinType::GeometryBuffer), IM_COL32(0, 229, 255, 255));
    EXPECT_EQ(NodeGraphEditorPanel::GetPinColor(PinType::Material),       IM_COL32(255, 215, 0, 255));
    EXPECT_EQ(NodeGraphEditorPanel::GetPinColor(PinType::Float),          IM_COL32(220, 220, 220, 255));
    EXPECT_EQ(NodeGraphEditorPanel::GetPinColor(PinType::Vector3),        IM_COL32(255, 111, 97, 255));
    EXPECT_EQ(NodeGraphEditorPanel::GetPinColor(PinType::Matrix4),        IM_COL32(255, 140, 0, 255));
}

TEST(NodeGraphEditorUITest, DomainHeaderColorStylingMatchesDomainType) {
    EXPECT_EQ(NodeGraphEditorPanel::GetDomainHeaderColor(NodeDomain::Geometry),  IM_COL32(27, 79, 114, 255));
    EXPECT_EQ(NodeGraphEditorPanel::GetDomainHeaderColor(NodeDomain::Material),  IM_COL32(30, 132, 73, 255));
    EXPECT_EQ(NodeGraphEditorPanel::GetDomainHeaderColor(NodeDomain::Animation), IM_COL32(108, 52, 131, 255));
}

TEST(NodeGraphEditorUITest, CubicBezierTangentLengthCalculation) {
    ImVec2 p0(100.0f, 100.0f);
    ImVec2 p3(300.0f, 200.0f);
    float tangent = NodeGraphEditorPanel::CalculateTangentLength(p0, p3);
    EXPECT_FLOAT_EQ(tangent, 100.0f);

    ImVec2 p0Close(100.0f, 100.0f);
    ImVec2 p3Close(120.0f, 100.0f);
    float tangentClose = NodeGraphEditorPanel::CalculateTangentLength(p0Close, p3Close);
    EXPECT_FLOAT_EQ(tangentClose, 50.0f);
}

TEST(NodeGraphEditorUITest, CanvasToScreenSpaceAndScreenToCanvasTransformations) {
    VulkanContext context(nullptr);
    NodeGraphEditorPanel panel(context);

    panel.SetPanOffset(ImVec2(50.0f, 30.0f));
    panel.SetZoom(1.5f);

    ImVec2 canvasOrigin(200.0f, 150.0f);
    ImVec2 canvasPos(100.0f, 80.0f);

    ImVec2 screenPos = panel.CanvasToScreenSpace(canvasPos, canvasOrigin);
    EXPECT_FLOAT_EQ(screenPos.x, 400.0f);
    EXPECT_FLOAT_EQ(screenPos.y, 300.0f);

    ImVec2 convertedCanvasPos = panel.ScreenToCanvasSpace(screenPos, canvasOrigin);
    EXPECT_FLOAT_EQ(convertedCanvasPos.x, 100.0f);
    EXPECT_FLOAT_EQ(convertedCanvasPos.y, 80.0f);
}

TEST(NodeGraphEditorUITest, NodePositionsCanBeSetAndRetrieved) {
    VulkanContext context(nullptr);
    NodeGraphEditorPanel panel(context);

    panel.SetNodePosition(42, ImVec2(150.0f, 220.0f));
    ImVec2 pos = panel.GetNodePosition(42);
    EXPECT_FLOAT_EQ(pos.x, 150.0f);
    EXPECT_FLOAT_EQ(pos.y, 220.0f);
}

TEST(NodeGraphEditorUITest, FloatNodeAndVector3NodeEvaluationAndDataflow) {
    NodeGraph graph;
    auto floatNode = graph.CreateNode<FloatNode>(90.0f);
    auto vecNode   = graph.CreateNode<Vector3Node>(glm::vec3(1.0f, 2.0f, 3.0f));
    auto twistNode = graph.CreateNode<TwistDeformerNode>();

    graph.Evaluate();
    EXPECT_FLOAT_EQ(floatNode->GetValue(), 90.0f);
    EXPECT_EQ(vecNode->GetValue(), glm::vec3(1.0f, 2.0f, 3.0f));

    // Connect FloatNode -> TwistDeformerNode Angle Pin
    const auto* floatOutPin = floatNode->FindOutput("Value");
    const auto* twistAnglePin = twistNode->FindInput("Angle");
    ASSERT_NE(floatOutPin, nullptr);
    ASSERT_NE(twistAnglePin, nullptr);

    EXPECT_TRUE(graph.Connect(floatOutPin->id, twistAnglePin->id));
    graph.Evaluate();

    EXPECT_FLOAT_EQ(twistNode->GetAngle(), 90.0f);
}

TEST(NodeGraphEditorUITest, AutoConnectNodePinWiresCompatiblePins) {
    VulkanContext context(nullptr);
    NodeGraphEditorPanel panel(context);
    auto graph = panel.GetGraph();

    auto floatNode = graph->CreateNode<FloatNode>(180.0f);
    auto twistNode = graph->CreateNode<TwistDeformerNode>(&context);

    const auto* floatOutPin = floatNode->FindOutput("Value");
    ASSERT_NE(floatOutPin, nullptr);

    // Auto connect float pin to twist node
    EXPECT_TRUE(panel.AutoConnectNodePin(floatOutPin->id, twistNode->GetId()));
    EXPECT_FLOAT_EQ(twistNode->GetAngle(), 180.0f);
}

TEST(NodeGraphEditorUITest, InlinePinValueMutationUpdatesNodeAndGraphState) {
    VulkanContext context(nullptr);
    NodeGraphEditorPanel panel(context);
    auto graph = panel.GetGraph();

    auto subdivNode = graph->CreateNode<SubdivisionNode>(&context, 1);
    auto twistNode  = graph->CreateNode<TwistDeformerNode>(&context);

    // Mutate Subdivision Level input pin directly
    auto* lvlPin = subdivNode->FindInput("Level");
    ASSERT_NE(lvlPin, nullptr);
    lvlPin->value = 3.0f;
    subdivNode->SetSubdivisionLevel(3);
    graph->Evaluate();

    EXPECT_EQ(subdivNode->GetSubdivisionLevel(), 3u);

    // Mutate Twist Angle input pin directly
    auto* anglePin = twistNode->FindInput("Angle");
    ASSERT_NE(anglePin, nullptr);
    anglePin->value = 120.0f;
    twistNode->SetAngle(120.0f);
    graph->Evaluate();

    EXPECT_FLOAT_EQ(twistNode->GetAngle(), 120.0f);
}

TEST(NodeGraphEditorUITest, FloatNodeAndVector3NodeValuePinMutation) {
    NodeGraph graph;
    auto floatNode = graph.CreateNode<FloatNode>(42.0f);
    auto vecNode   = graph.CreateNode<Vector3Node>(glm::vec3(1.5f, -2.0f, 3.2f));

    floatNode->SetValue(84.0f);
    vecNode->SetValue(glm::vec3(4.0f, 5.0f, 6.0f));
    graph.Evaluate();

    const auto* floatOutPin = floatNode->FindOutput("Value");
    const auto* vecOutPin   = vecNode->FindOutput("Value");

    ASSERT_NE(floatOutPin, nullptr);
    ASSERT_NE(vecOutPin, nullptr);

    EXPECT_TRUE(std::holds_alternative<float>(floatOutPin->value));
    EXPECT_FLOAT_EQ(std::get<float>(floatOutPin->value), 84.0f);

    EXPECT_TRUE(std::holds_alternative<glm::vec3>(vecOutPin->value));
    EXPECT_EQ(std::get<glm::vec3>(vecOutPin->value), glm::vec3(4.0f, 5.0f, 6.0f));
}

#include "scene/SceneNode.h"

TEST(NodeGraphEditorUITest, PerSceneNodeGraphIsolation) {
    VulkanContext context(nullptr);
    NodeGraphEditorPanel panel(context);

    auto sceneNodeA = std::make_unique<SceneNode>("Cube Node");
    auto sceneNodeB = std::make_unique<SceneNode>("Bunny Node");

    // Assign target A
    panel.SetTargetSceneNode(sceneNodeA.get());
    EXPECT_EQ(panel.GetTargetSceneNode(), sceneNodeA.get());
    auto graphA = panel.GetGraph();
    ASSERT_NE(graphA, nullptr);
    auto primA = graphA->CreateNode<MeshPrimitiveNode>(&context, MeshPrimitiveNode::PrimitiveType::Cube);

    // Switch target to B
    panel.SetTargetSceneNode(sceneNodeB.get());
    EXPECT_EQ(panel.GetTargetSceneNode(), sceneNodeB.get());
    auto graphB = panel.GetGraph();
    ASSERT_NE(graphB, nullptr);
    EXPECT_NE(graphA, graphB);

    // Graph B should not have primA
    EXPECT_EQ(graphB->GetNode(primA->GetId()), nullptr);

    // Switch back to A and verify graphA intact
    panel.SetTargetSceneNode(sceneNodeA.get());
    EXPECT_NE(panel.GetGraph()->GetNode(primA->GetId()), nullptr);
}

TEST(NodeGraphEditorUITest, MultiNodeSelectionAndTranslation) {
    VulkanContext context(nullptr);
    NodeGraphEditorPanel panel(context);

    panel.ClearSelection();
    EXPECT_TRUE(panel.GetSelectedNodeIds().empty());

    // Select node 1
    panel.SelectNode(1, false);
    EXPECT_TRUE(panel.IsNodeSelected(1));
    EXPECT_EQ(panel.GetSelectedNodeIds().size(), 1u);

    // Add node 2 with multi-selection
    panel.SelectNode(2, true);
    EXPECT_TRUE(panel.IsNodeSelected(1));
    EXPECT_TRUE(panel.IsNodeSelected(2));
    EXPECT_EQ(panel.GetSelectedNodeIds().size(), 2u);

    // Deselect node 1
    panel.DeselectNode(1);
    EXPECT_FALSE(panel.IsNodeSelected(1));
    EXPECT_TRUE(panel.IsNodeSelected(2));
    EXPECT_EQ(panel.GetSelectedNodeIds().size(), 1u);

    // Clear all
    panel.ClearSelection();
    EXPECT_FALSE(panel.IsNodeSelected(2));
    EXPECT_TRUE(panel.GetSelectedNodeIds().empty());
}

TEST(NodeGraphEditorUITest, DisplayFlagPreviewMode) {
    VulkanContext context(nullptr);
    NodeGraphEditorPanel panel(context);

    EXPECT_EQ(panel.GetPreviewNodeId(), 0u);

    panel.SetPreviewNodeId(2);
    EXPECT_EQ(panel.GetPreviewNodeId(), 2u);

    panel.SetPreviewNodeId(0);
    EXPECT_EQ(panel.GetPreviewNodeId(), 0u);
}

TEST(NodeGraphEditorUITest, GroupDeformationPropagationFlag) {
    VulkanContext context(nullptr);
    TwistDeformerNode twist(1, &context);
    SubdivisionNode subdiv(2, &context, 2);
    CSGBooleanNode csg(3, &context);

    EXPECT_FALSE(twist.GetApplyToChildren());
    EXPECT_FALSE(subdiv.GetApplyToChildren());
    EXPECT_FALSE(csg.GetApplyToChildren());

    twist.SetApplyToChildren(true);
    subdiv.SetApplyToChildren(true);
    csg.SetApplyToChildren(true);

    EXPECT_TRUE(twist.GetApplyToChildren());
    EXPECT_TRUE(subdiv.GetApplyToChildren());
    EXPECT_TRUE(csg.GetApplyToChildren());
}


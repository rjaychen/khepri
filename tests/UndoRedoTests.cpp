#include <gtest/gtest.h>
#include <variant>
#include "../src/core/Command.h"
#include "../src/core/UndoStack.h"
#include "../src/scene/SceneNode.h"
#include "../src/scene/TransformCommand.h"
#include "../src/scene/SceneHierarchyCommand.h"
#include "../src/graph/NodeGraph.h"
#include "../src/graph/GeometryNodes.h"
#include "../src/graph/NodeGraphCommand.h"
#include "../src/animation/Timeline.h"
#include "../src/animation/KeyframeCommand.h"

using namespace khepri;

// ============================================================================
// UndoStack Core Unit Tests
// ============================================================================

TEST(UndoStackTest, DefaultStateIsEmpty) {
    core::UndoStack stack;
    EXPECT_FALSE(stack.CanUndo());
    EXPECT_FALSE(stack.CanRedo());
    EXPECT_EQ(stack.GetUndoCount(), 0u);
    EXPECT_EQ(stack.GetRedoCount(), 0u);
    EXPECT_TRUE(stack.GetUndoCommandName().empty());
    EXPECT_TRUE(stack.GetRedoCommandName().empty());
}

TEST(UndoStackTest, PushAndExecuteSingleCommand) {
    core::UndoStack stack;
    int state = 0;

    auto cmd = std::make_unique<core::LambdaCommand>(
        "Increment State",
        [&state]() { state += 10; },
        [&state]() { state -= 10; }
    );

    stack.PushAndExecute(std::move(cmd));
    EXPECT_EQ(state, 10);
    EXPECT_TRUE(stack.CanUndo());
    EXPECT_FALSE(stack.CanRedo());
    EXPECT_EQ(stack.GetUndoCount(), 1u);
    EXPECT_EQ(stack.GetUndoCommandName(), "Increment State");

    EXPECT_TRUE(stack.Undo());
    EXPECT_EQ(state, 0);
    EXPECT_FALSE(stack.CanUndo());
    EXPECT_TRUE(stack.CanRedo());
    EXPECT_EQ(stack.GetRedoCount(), 1u);
    EXPECT_EQ(stack.GetRedoCommandName(), "Increment State");

    EXPECT_TRUE(stack.Redo());
    EXPECT_EQ(state, 10);
    EXPECT_TRUE(stack.CanUndo());
    EXPECT_FALSE(stack.CanRedo());
}

TEST(UndoStackTest, PushAlreadyExecutedCommand) {
    core::UndoStack stack;
    int state = 5; // already executed manually

    auto cmd = std::make_unique<core::LambdaCommand>(
        "Pre-Executed",
        [&state]() { state = 5; },
        [&state]() { state = 0; }
    );

    stack.Push(std::move(cmd));
    EXPECT_EQ(state, 5); // Execute() not invoked again on push
    EXPECT_TRUE(stack.CanUndo());

    stack.Undo();
    EXPECT_EQ(state, 0);

    stack.Redo();
    EXPECT_EQ(state, 5);
}

TEST(UndoStackTest, MaxCapacityFIFOEviction) {
    core::UndoStack stack(3); // Max 3 items
    int counter = 0;

    for (int i = 1; i <= 5; ++i) {
        stack.PushAndExecute(std::make_unique<core::LambdaCommand>(
            "Step " + std::to_string(i),
            [&counter, i]() { counter = i; },
            [&counter, i]() { counter = i - 1; }
        ));
    }

    EXPECT_EQ(stack.GetUndoCount(), 3u);
    EXPECT_EQ(counter, 5);

    // Can only undo 3 times (steps 5, 4, 3)
    EXPECT_TRUE(stack.Undo()); // 5 -> 4
    EXPECT_EQ(counter, 4);
    EXPECT_TRUE(stack.Undo()); // 4 -> 3
    EXPECT_EQ(counter, 3);
    EXPECT_TRUE(stack.Undo()); // 3 -> 2
    EXPECT_EQ(counter, 2);
    EXPECT_FALSE(stack.CanUndo()); // Older steps 1 and 2 were pruned
}

TEST(UndoStackTest, NewActionClearsRedoStack) {
    core::UndoStack stack;
    int val = 0;

    stack.PushAndExecute(std::make_unique<core::LambdaCommand>("Cmd 1", [&val]() { val = 1; }, [&val]() { val = 0; }));
    stack.PushAndExecute(std::make_unique<core::LambdaCommand>("Cmd 2", [&val]() { val = 2; }, [&val]() { val = 1; }));
    stack.PushAndExecute(std::make_unique<core::LambdaCommand>("Cmd 3", [&val]() { val = 3; }, [&val]() { val = 2; }));

    EXPECT_EQ(val, 3);
    stack.Undo(); // val = 2
    stack.Undo(); // val = 1
    EXPECT_EQ(val, 1);
    EXPECT_EQ(stack.GetRedoCount(), 2u);

    // Push new command
    stack.PushAndExecute(std::make_unique<core::LambdaCommand>("Cmd 4", [&val]() { val = 100; }, [&val]() { val = 1; }));
    EXPECT_EQ(val, 100);
    EXPECT_EQ(stack.GetRedoCount(), 0u);
    EXPECT_FALSE(stack.CanRedo());
}

TEST(UndoStackTest, CompoundTransactionCommit) {
    core::UndoStack stack;
    int a = 0;
    int b = 0;

    stack.BeginTransaction("Batch Delta");
    EXPECT_TRUE(stack.IsInTransaction());
    EXPECT_FALSE(stack.CanUndo());

    stack.PushAndExecute(std::make_unique<core::LambdaCommand>("Inc A", [&a]() { a += 5; }, [&a]() { a -= 5; }));
    stack.PushAndExecute(std::make_unique<core::LambdaCommand>("Inc B", [&b]() { b += 10; }, [&b]() { b -= 10; }));

    EXPECT_EQ(a, 5);
    EXPECT_EQ(b, 10);

    stack.EndTransaction();
    EXPECT_FALSE(stack.IsInTransaction());
    EXPECT_TRUE(stack.CanUndo());
    EXPECT_EQ(stack.GetUndoCount(), 1u);
    EXPECT_EQ(stack.GetUndoCommandName(), "Batch Delta");

    // Single undo reverts both operations
    stack.Undo();
    EXPECT_EQ(a, 0);
    EXPECT_EQ(b, 0);

    // Single redo reapplies both operations
    stack.Redo();
    EXPECT_EQ(a, 5);
    EXPECT_EQ(b, 10);
}

TEST(UndoStackTest, CancelTransactionRevertsExecutedSteps) {
    core::UndoStack stack;
    int x = 0;

    stack.BeginTransaction("Aborted Op");
    stack.PushAndExecute(std::make_unique<core::LambdaCommand>("Set X 42", [&x]() { x = 42; }, [&x]() { x = 0; }));
    EXPECT_EQ(x, 42);

    stack.CancelTransaction();
    EXPECT_EQ(x, 0);
    EXPECT_FALSE(stack.CanUndo());
    EXPECT_EQ(stack.GetUndoCount(), 0u);
}

TEST(UndoStackTest, ChangeListenerCallbackTriggers) {
    core::UndoStack stack;
    int callbackCount = 0;

    stack.SetChangeListener([&callbackCount]() {
        callbackCount++;
    });

    stack.PushAndExecute(std::make_unique<core::LambdaCommand>("Cmd", []() {}, []() {}));
    EXPECT_EQ(callbackCount, 1);

    stack.Undo();
    EXPECT_EQ(callbackCount, 2);

    stack.Redo();
    EXPECT_EQ(callbackCount, 3);

    stack.Clear();
    EXPECT_EQ(callbackCount, 4);
}

TEST(UndoStackTest, CleanStateSaveTracking) {
    core::UndoStack stack;
    EXPECT_TRUE(stack.IsClean());
    EXPECT_FALSE(stack.IsDirty());

    int counter = 0;
    stack.PushAndExecute(std::make_unique<core::LambdaCommand>("Inc", [&]() { counter++; }, [&]() { counter--; }));
    EXPECT_FALSE(stack.IsClean());
    EXPECT_TRUE(stack.IsDirty());

    // Mark clean (e.g. user saved document)
    stack.MarkClean();
    EXPECT_TRUE(stack.IsClean());
    EXPECT_FALSE(stack.IsDirty());

    // Undo takes it back to dirty (relative to saved version)
    stack.Undo();
    EXPECT_FALSE(stack.IsClean());
    EXPECT_TRUE(stack.IsDirty());

    // Redo restores to clean
    stack.Redo();
    EXPECT_TRUE(stack.IsClean());
    EXPECT_FALSE(stack.IsDirty());
}

TEST(UndoStackTest, ScopedTransactionAutoCommitAndCancel) {
    core::UndoStack stack;
    int a = 0, b = 0;

    // Scope 1: Auto-commits on destructor
    {
        core::ScopedTransaction tx(stack, "Batch Action");
        stack.PushAndExecute(std::make_unique<core::LambdaCommand>("A", [&]() { a += 5; }, [&]() { a -= 5; }));
        stack.PushAndExecute(std::make_unique<core::LambdaCommand>("B", [&]() { b += 10; }, [&]() { b -= 10; }));
    }

    EXPECT_EQ(stack.GetUndoCount(), 1u);
    EXPECT_EQ(a, 5);
    EXPECT_EQ(b, 10);

    stack.Undo();
    EXPECT_EQ(a, 0);
    EXPECT_EQ(b, 0);

    // Scope 2: Explicit cancel
    {
        core::ScopedTransaction tx(stack, "Cancelled Action");
        stack.PushAndExecute(std::make_unique<core::LambdaCommand>("A", [&]() { a += 100; }, [&]() { a -= 100; }));
        EXPECT_EQ(a, 100);
        tx.Cancel();
    }

    EXPECT_EQ(a, 0);
    EXPECT_EQ(stack.GetUndoCount(), 0u);
}

// ============================================================================
// TransformCommand Tests
// ============================================================================

TEST(TransformCommandTest, PositionRotationScaleUndoRedoAndPropertySync) {
    core::UndoStack stack;
    SceneNode node("TestNode");

    glm::vec3 origPos(0.0f, 0.0f, 0.0f);
    glm::vec3 origRot(0.0f, 0.0f, 0.0f);
    glm::vec3 origScale(1.0f, 1.0f, 1.0f);

    glm::vec3 newPos(10.0f, 2.0f, -5.0f);
    glm::vec3 newRot(45.0f, 90.0f, 0.0f);
    glm::vec3 newScale(2.0f, 2.0f, 2.0f);

    auto cmd = std::make_unique<scene::TransformCommand>(
        &node, origPos, origRot, origScale, newPos, newRot, newScale
    );

    stack.PushAndExecute(std::move(cmd));

    EXPECT_FLOAT_EQ(node.position.x, 10.0f);
    EXPECT_FLOAT_EQ(node.position.y, 2.0f);
    EXPECT_FLOAT_EQ(node.position.z, -5.0f);
    EXPECT_FLOAT_EQ(node.rotationDegrees.y, 90.0f);
    EXPECT_FLOAT_EQ(node.scale.x, 2.0f);

    // Verify properties synchronized
    const auto& props = node.GetProperties();
    ASSERT_GE(props.size(), 1u);
    EXPECT_NEAR(props[0].GetValue<glm::vec3>().x, 10.0f, 1e-4f);

    stack.Undo();
    EXPECT_FLOAT_EQ(node.position.x, 0.0f);
    EXPECT_FLOAT_EQ(node.rotationDegrees.y, 0.0f);
    EXPECT_FLOAT_EQ(node.scale.x, 1.0f);

    stack.Redo();
    EXPECT_FLOAT_EQ(node.position.x, 10.0f);
}

TEST(TransformCommandTest, CommandMergingOnContinuousDrag) {
    core::UndoStack stack;
    SceneNode node("DraggableNode");

    glm::vec3 p0(0.0f);
    glm::vec3 p1(1.0f, 0.0f, 0.0f);
    glm::vec3 p2(2.0f, 0.0f, 0.0f);
    glm::vec3 p3(3.0f, 0.0f, 0.0f);
    glm::vec3 r(0.0f), s(1.0f);

    stack.PushAndExecute(std::make_unique<scene::TransformCommand>(&node, p0, r, s, p1, r, s));
    stack.PushAndExecute(std::make_unique<scene::TransformCommand>(&node, p1, r, s, p2, r, s));
    stack.PushAndExecute(std::make_unique<scene::TransformCommand>(&node, p2, r, s, p3, r, s));

    // Because all 3 commands target the same node consecutively, they should merge into 1 command
    EXPECT_EQ(stack.GetUndoCount(), 1u);
    EXPECT_FLOAT_EQ(node.position.x, 3.0f);

    stack.Undo();
    EXPECT_FLOAT_EQ(node.position.x, 0.0f);

    stack.Redo();
    EXPECT_FLOAT_EQ(node.position.x, 3.0f);
}

// ============================================================================
// SceneHierarchyCommand Tests
// ============================================================================

TEST(SceneHierarchyCommandTest, AddAndRemoveChildNode) {
    core::UndoStack stack;
    SceneNode root("Root");

    auto child = std::make_unique<SceneNode>("ChildObject");
    SceneNode* rawChild = child.get();

    stack.PushAndExecute(std::make_unique<scene::AddChildNodeCommand>(&root, std::move(child)));

    EXPECT_EQ(root.GetChildren().size(), 1u);
    EXPECT_TRUE(root.Contains(rawChild));

    stack.Undo();
    EXPECT_EQ(root.GetChildren().size(), 0u);
    EXPECT_FALSE(root.Contains(rawChild));

    stack.Redo();
    EXPECT_EQ(root.GetChildren().size(), 1u);
}

TEST(SceneHierarchyCommandTest, DeleteChildNodeWithUndoRestoration) {
    core::UndoStack stack;
    SceneNode root("Root");

    auto child = std::make_unique<SceneNode>("TargetToDelete");
    SceneNode* rawChild = root.AddChild(std::move(child));
    ASSERT_EQ(root.GetChildren().size(), 1u);

    stack.PushAndExecute(std::make_unique<scene::RemoveChildNodeCommand>(&root, rawChild));
    EXPECT_EQ(root.GetChildren().size(), 0u);

    stack.Undo();
    EXPECT_EQ(root.GetChildren().size(), 1u);
    EXPECT_EQ(root.GetChildren()[0]->name, "TargetToDelete");

    stack.Redo();
    EXPECT_EQ(root.GetChildren().size(), 0u);
}

TEST(SceneHierarchyCommandTest, ReparentAndRenameNode) {
    core::UndoStack stack;
    SceneNode root("Root");
    SceneNode* parentA = root.AddChild(std::make_unique<SceneNode>("ParentA"));
    SceneNode* parentB = root.AddChild(std::make_unique<SceneNode>("ParentB"));
    SceneNode* item = parentA->AddChild(std::make_unique<SceneNode>("Item"));

    EXPECT_TRUE(parentA->Contains(item));
    EXPECT_FALSE(parentB->Contains(item));

    // Reparent from A to B
    stack.PushAndExecute(std::make_unique<scene::ReparentNodeCommand>(item, parentB));
    EXPECT_FALSE(parentA->Contains(item));
    EXPECT_TRUE(parentB->Contains(item));

    // Rename item
    stack.PushAndExecute(std::make_unique<scene::RenameNodeCommand>(item, "RenamedItem"));
    EXPECT_EQ(item->name, "RenamedItem");

    // Undo rename
    stack.Undo();
    EXPECT_EQ(item->name, "Item");

    // Undo reparent
    stack.Undo();
    EXPECT_TRUE(parentA->Contains(item));
    EXPECT_FALSE(parentB->Contains(item));
}

// ============================================================================
// NodeGraphCommand Tests
// ============================================================================

TEST(NodeGraphCommandTest, AddDeleteAndRestoreNodeWithConnections) {
    core::UndoStack stack;
    graph::NodeGraph graph;

    auto primNode = graph.CreateNode<graph::MeshPrimitiveNode>(nullptr, graph::MeshPrimitiveNode::PrimitiveType::Cube);
    auto subdivNode = graph.CreateNode<graph::SubdivisionNode>(nullptr, 1);

    uint32_t outPinId = primNode->FindOutput("MeshBuffer")->id;
    uint32_t inPinId = subdivNode->FindInput("MeshBuffer")->id;

    stack.PushAndExecute(std::make_unique<graph::ConnectPinsCommand>(&graph, outPinId, inPinId));
    EXPECT_EQ(subdivNode->FindInput("MeshBuffer")->connectedPinId, outPinId);

    // Delete primitive node (which should disconnect the link)
    stack.PushAndExecute(std::make_unique<graph::DeleteNodeCommand>(&graph, primNode));
    EXPECT_EQ(graph.GetNodes().size(), 1u);
    EXPECT_EQ(subdivNode->FindInput("MeshBuffer")->connectedPinId, 0u);

    // Undo deletion: node and wire connection must be restored!
    stack.Undo();
    EXPECT_EQ(graph.GetNodes().size(), 2u);
    auto restoredPrim = graph.GetNode(primNode->GetId());
    ASSERT_NE(restoredPrim, nullptr);
    EXPECT_EQ(subdivNode->FindInput("MeshBuffer")->connectedPinId, outPinId);

    // Redo deletion
    stack.Redo();
    EXPECT_EQ(graph.GetNodes().size(), 1u);
    EXPECT_EQ(subdivNode->FindInput("MeshBuffer")->connectedPinId, 0u);
}

TEST(NodeGraphCommandTest, ChangePinValueUndoRedo) {
    core::UndoStack stack;
    graph::NodeGraph graph;

    auto floatNode = graph.CreateNode<graph::FloatNode>(3.14f);
    auto outPin = floatNode->FindOutput("Value");
    ASSERT_NE(outPin, nullptr);
    ASSERT_NE(std::get_if<float>(&outPin->value), nullptr);
    EXPECT_NEAR(*std::get_if<float>(&outPin->value), 3.14f, 1e-4f);

    stack.PushAndExecute(std::make_unique<graph::ChangePinValueCommand>(&graph, outPin->id, 3.14f, 9.99f));
    ASSERT_NE(std::get_if<float>(&outPin->value), nullptr);
    EXPECT_NEAR(*std::get_if<float>(&outPin->value), 9.99f, 1e-4f);

    stack.Undo();
    ASSERT_NE(std::get_if<float>(&outPin->value), nullptr);
    EXPECT_NEAR(*std::get_if<float>(&outPin->value), 3.14f, 1e-4f);

    stack.Redo();
    ASSERT_NE(std::get_if<float>(&outPin->value), nullptr);
    EXPECT_NEAR(*std::get_if<float>(&outPin->value), 9.99f, 1e-4f);
}

// ============================================================================
// KeyframeCommand Tests
// ============================================================================

TEST(KeyframeCommandTest, AddRemoveAndUndoKeyframes) {
    core::UndoStack stack;
    auto clip = std::make_shared<AnimationClip>();
    clip->duration = 5.0f;

    glm::vec3 posA(0.0f, 0.0f, 0.0f);
    glm::vec3 posB(10.0f, 5.0f, 0.0f);

    stack.PushAndExecute(std::make_unique<animation::AddPositionKeyframeCommand>(clip, "Target", 0.0f, posA));
    stack.PushAndExecute(std::make_unique<animation::AddPositionKeyframeCommand>(clip, "Target", 2.0f, posB));

    auto track = clip->GetOrCreateTrack("Target");
    ASSERT_NE(track, nullptr);
    EXPECT_EQ(track->positionKeys.size(), 2u);
    EXPECT_FLOAT_EQ(track->SamplePosition(1.0f).x, 5.0f);

    // Undo second keyframe
    stack.Undo();
    EXPECT_EQ(track->positionKeys.size(), 1u);
    EXPECT_FLOAT_EQ(track->SamplePosition(1.0f).x, 0.0f);

    // Redo second keyframe
    stack.Redo();
    EXPECT_EQ(track->positionKeys.size(), 2u);
    EXPECT_FLOAT_EQ(track->SamplePosition(1.0f).x, 5.0f);

    // Remove first keyframe
    stack.PushAndExecute(std::make_unique<animation::RemoveKeyframeCommand<glm::vec3>>(clip, "Target", 0.0f));
    EXPECT_EQ(track->positionKeys.size(), 1u);
    EXPECT_FLOAT_EQ(track->positionKeys[0].time, 2.0f);

    // Undo removal
    stack.Undo();
    EXPECT_EQ(track->positionKeys.size(), 2u);
    EXPECT_FLOAT_EQ(track->positionKeys[0].time, 0.0f);
}

// ============================================================================
// Comprehensive Hierarchy & Node Graph Integration Tests
// ============================================================================

TEST(SceneHierarchyCommandTest, SetVisibilityCommandUndoRedo) {
    core::UndoStack stack;
    SceneNode node("TestVisibility");
    EXPECT_TRUE(node.visible);

    stack.PushAndExecute(std::make_unique<scene::SetNodeVisibilityCommand>(&node, false));
    EXPECT_FALSE(node.visible);

    stack.Undo();
    EXPECT_TRUE(node.visible);

    stack.Redo();
    EXPECT_FALSE(node.visible);
}

TEST(SceneHierarchyCommandTest, NodeDeletionAndHierarchyIntegrity) {
    core::UndoStack stack;
    SceneNode root("Root");

    auto childA = std::make_unique<SceneNode>("ChildA");
    auto* rawA = root.AddChild(std::move(childA));
    auto childB = std::make_unique<SceneNode>("ChildB");
    auto* rawB = rawA->AddChild(std::move(childB));

    ASSERT_TRUE(root.Contains(rawA));
    ASSERT_TRUE(root.Contains(rawB));

    // Delete ChildA (which also detaches its descendant ChildB)
    stack.PushAndExecute(std::make_unique<scene::RemoveChildNodeCommand>(&root, rawA));
    EXPECT_FALSE(root.Contains(rawA));
    EXPECT_FALSE(root.Contains(rawB));

    // Undo: Both ChildA and ChildB are restored
    stack.Undo();
    EXPECT_TRUE(root.Contains(rawA));
    EXPECT_TRUE(root.Contains(rawB));
    EXPECT_EQ(rawA->GetChildren().size(), 1u);
    EXPECT_EQ(rawA->GetChildren()[0]->name, "ChildB");

    // Redo
    stack.Redo();
    EXPECT_FALSE(root.Contains(rawA));
    EXPECT_FALSE(root.Contains(rawB));
}

TEST(NodeGraphCommandTest, BatchDeleteWithPositionsAndScopedTransaction) {
    core::UndoStack stack;
    graph::NodeGraph graph;
    std::unordered_map<uint32_t, ImVec2> positionsMap;

    auto nodeA = graph.CreateNode<graph::MeshPrimitiveNode>(nullptr, graph::MeshPrimitiveNode::PrimitiveType::Cube);
    auto nodeB = graph.CreateNode<graph::SubdivisionNode>(nullptr, 1);
    uint32_t idA = nodeA->GetId();
    uint32_t idB = nodeB->GetId();

    positionsMap[idA] = ImVec2(100.0f, 200.0f);
    positionsMap[idB] = ImVec2(400.0f, 200.0f);

    uint32_t outPinId = nodeA->FindOutput("MeshBuffer")->id;
    nodeB->FindInput("MeshBuffer")->connectedPinId = outPinId;
    EXPECT_EQ(nodeB->FindInput("MeshBuffer")->connectedPinId, outPinId);

    // Delete both nodes in a single atomic transaction
    {
        core::ScopedTransaction tx(stack, "Delete 2 Nodes");
        stack.PushAndExecute(std::make_unique<graph::DeleteNodeCommand>(&graph, nodeA, &positionsMap));
        stack.PushAndExecute(std::make_unique<graph::DeleteNodeCommand>(&graph, nodeB, &positionsMap));
    }

    EXPECT_EQ(stack.GetUndoCount(), 1u);
    EXPECT_EQ(graph.GetNodes().size(), 0u);
    EXPECT_EQ(positionsMap.find(idA), positionsMap.end());
    EXPECT_EQ(positionsMap.find(idB), positionsMap.end());

    // Single Undo restores both nodes, their wire connection, and canvas positions
    stack.Undo();
    EXPECT_EQ(graph.GetNodes().size(), 2u);
    auto restoredA = graph.GetNode(idA);
    auto restoredB = graph.GetNode(idB);
    ASSERT_NE(restoredA, nullptr);
    ASSERT_NE(restoredB, nullptr);
    EXPECT_EQ(restoredB->FindInput("MeshBuffer")->connectedPinId, outPinId);
    ASSERT_NE(positionsMap.find(idA), positionsMap.end());
    ASSERT_NE(positionsMap.find(idB), positionsMap.end());
    EXPECT_FLOAT_EQ(positionsMap[idA].x, 100.0f);
    EXPECT_FLOAT_EQ(positionsMap[idA].y, 200.0f);
    EXPECT_FLOAT_EQ(positionsMap[idB].x, 400.0f);
    EXPECT_FLOAT_EQ(positionsMap[idB].y, 200.0f);

    // Redo deletes both again
    stack.Redo();
    EXPECT_EQ(graph.GetNodes().size(), 0u);
    EXPECT_EQ(positionsMap.find(idA), positionsMap.end());
    EXPECT_EQ(positionsMap.find(idB), positionsMap.end());
}

TEST(NodeGraphCommandTest, MoveNodesCommandUndoRedo) {
    core::UndoStack stack;
    std::unordered_map<uint32_t, ImVec2> positionsMap;
    positionsMap[1] = ImVec2(50.0f, 60.0f);
    positionsMap[2] = ImVec2(150.0f, 160.0f);

    std::vector<graph::MoveNodesCommand::NodePositionRecord> records = {
        { 1, glm::vec2(0.0f, 0.0f), glm::vec2(50.0f, 60.0f) },
        { 2, glm::vec2(100.0f, 100.0f), glm::vec2(150.0f, 160.0f) }
    };

    auto cmd = std::make_unique<graph::MoveNodesCommand>(&positionsMap, std::move(records));
    stack.Push(std::move(cmd)); // Already at endPos

    EXPECT_FLOAT_EQ(positionsMap[1].x, 50.0f);
    EXPECT_FLOAT_EQ(positionsMap[1].y, 60.0f);

    stack.Undo();
    EXPECT_FLOAT_EQ(positionsMap[1].x, 0.0f);
    EXPECT_FLOAT_EQ(positionsMap[1].y, 0.0f);
    EXPECT_FLOAT_EQ(positionsMap[2].x, 100.0f);
    EXPECT_FLOAT_EQ(positionsMap[2].y, 100.0f);

    stack.Redo();
    EXPECT_FLOAT_EQ(positionsMap[1].x, 50.0f);
    EXPECT_FLOAT_EQ(positionsMap[1].y, 60.0f);
    EXPECT_FLOAT_EQ(positionsMap[2].x, 150.0f);
    EXPECT_FLOAT_EQ(positionsMap[2].y, 160.0f);
}

TEST(NodeGraphCommandTest, DisconnectPinsCommandUndoRedo) {
    core::UndoStack stack;
    graph::NodeGraph graph;

    auto nodeA = graph.CreateNode<graph::MeshPrimitiveNode>(nullptr, graph::MeshPrimitiveNode::PrimitiveType::Cube);
    auto nodeB = graph.CreateNode<graph::SubdivisionNode>(nullptr, 1);
    uint32_t outPinId = nodeA->FindOutput("MeshBuffer")->id;
    uint32_t inPinId = nodeB->FindInput("MeshBuffer")->id;

    nodeB->FindInput("MeshBuffer")->connectedPinId = outPinId;
    EXPECT_EQ(nodeB->FindInput("MeshBuffer")->connectedPinId, outPinId);

    // Disconnect wire via command
    stack.PushAndExecute(std::make_unique<graph::DisconnectPinsCommand>(&graph, inPinId));
    EXPECT_EQ(nodeB->FindInput("MeshBuffer")->connectedPinId, 0u);

    // Undo disconnect: wire reconnects
    stack.Undo();
    EXPECT_EQ(nodeB->FindInput("MeshBuffer")->connectedPinId, outPinId);

    // Redo disconnect
    stack.Redo();
    EXPECT_EQ(nodeB->FindInput("MeshBuffer")->connectedPinId, 0u);
}

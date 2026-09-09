#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../src/editor/SceneTreePanel.h"
#include "../src/scene/SceneNode.h"
#include "../src/core/UndoStack.h"
#include "../src/scene/SceneHierarchyCommand.h"

// ---------------------------------------------------------------------------
// Scene Tree Panel & Multi-Selection Unit Tests
// ---------------------------------------------------------------------------

TEST(SceneTreeOverhaulTest, MultiSelectionStateOperations) {
    SceneTreePanel panel;
    auto root = std::make_shared<SceneNode>("Root");
    auto nodeA = root->AddChild(std::make_unique<SceneNode>("Node A"));
    auto nodeB = root->AddChild(std::make_unique<SceneNode>("Node B"));
    auto nodeC = root->AddChild(std::make_unique<SceneNode>("Node C"));

    // Default state
    EXPECT_EQ(panel.GetSelectedNode(), nullptr);
    EXPECT_TRUE(panel.GetSelectedNodes().empty());

    // Single select node A
    panel.SelectNode(nodeA, false);
    EXPECT_EQ(panel.GetSelectedNode(), nodeA);
    EXPECT_TRUE(panel.IsNodeSelected(nodeA));
    EXPECT_FALSE(panel.IsNodeSelected(nodeB));
    EXPECT_EQ(panel.GetSelectedNodes().size(), 1u);

    // Additive select node B (Ctrl+Click)
    panel.SelectNode(nodeB, true);
    EXPECT_TRUE(panel.IsNodeSelected(nodeA));
    EXPECT_TRUE(panel.IsNodeSelected(nodeB));
    EXPECT_FALSE(panel.IsNodeSelected(nodeC));
    EXPECT_EQ(panel.GetSelectedNodes().size(), 2u);

    // Deselect node A
    panel.DeselectNode(nodeA);
    EXPECT_FALSE(panel.IsNodeSelected(nodeA));
    EXPECT_TRUE(panel.IsNodeSelected(nodeB));
    EXPECT_EQ(panel.GetSelectedNodes().size(), 1u);

    // Select All (strictly excludes root)
    panel.SelectAll(root.get());
    EXPECT_FALSE(panel.IsNodeSelected(root.get()));
    EXPECT_TRUE(panel.IsNodeSelected(nodeA));
    EXPECT_TRUE(panel.IsNodeSelected(nodeB));
    EXPECT_TRUE(panel.IsNodeSelected(nodeC));
    EXPECT_EQ(panel.GetSelectedNodes().size(), 3u);
    EXPECT_EQ(panel.GetSelectedNode(), nodeA);

    // Clear All
    panel.ClearAllSelections();
    EXPECT_EQ(panel.GetSelectedNode(), nullptr);
    EXPECT_TRUE(panel.GetSelectedNodes().empty());
}

TEST(SceneTreeOverhaulTest, SelectionValidationAfterNodeDeletion) {
    SceneTreePanel panel;
    auto root = std::make_shared<SceneNode>("Root");
    auto child = root->AddChild(std::make_unique<SceneNode>("Child"));

    panel.SelectNode(child);
    EXPECT_EQ(panel.GetSelectedNode(), child);

    // Delete child from root
    root->RemoveChild(child);

    // Validate
    panel.ValidateSelection(root.get());
    EXPECT_EQ(panel.GetSelectedNode(), nullptr);
    EXPECT_FALSE(panel.IsNodeSelected(child));
}

TEST(SceneTreeOverhaulTest, InlineRenamingTrigger) {
    SceneTreePanel panel;
    auto node = std::make_unique<SceneNode>("OriginalName");

    panel.StartRenaming(node.get());
    // Starting renaming does not crash and prepares internal buffer
}

TEST(SceneTreeOverhaulTest, SearchFilterMatching) {
    SceneTreePanel panel;
    panel.SetFilterText("mesh");
    EXPECT_STREQ(panel.GetFilterText(), "mesh");
}

TEST(SceneTreeOverhaulTest, MultiNodeBatchDeletionWithUndoStack) {
    khepri::core::UndoStack undoStack;
    auto root = std::make_shared<SceneNode>("Root");
    auto node1 = root->AddChild(std::make_unique<SceneNode>("Node 1"));
    auto node2 = root->AddChild(std::make_unique<SceneNode>("Node 2"));

    ASSERT_EQ(root->GetChildren().size(), 2u);

    // Batch delete with transaction
    undoStack.BeginTransaction("Batch Delete Nodes");
    undoStack.PushAndExecute(std::make_unique<khepri::scene::RemoveChildNodeCommand>(root.get(), node1));
    undoStack.PushAndExecute(std::make_unique<khepri::scene::RemoveChildNodeCommand>(root.get(), node2));
    undoStack.EndTransaction();

    EXPECT_EQ(root->GetChildren().size(), 0u);

    // Undo restores both nodes
    undoStack.Undo();
    EXPECT_EQ(root->GetChildren().size(), 2u);

    // Redo deletes both nodes again
    undoStack.Redo();
    EXPECT_EQ(root->GetChildren().size(), 0u);
}

TEST(SceneTreeOverhaulTest, FindDescendantById) {
    auto root = std::make_shared<SceneNode>("Root");
    auto child1 = root->AddChild(std::make_unique<SceneNode>("Child 1"));
    auto child2 = root->AddChild(std::make_unique<SceneNode>("Child 2"));
    auto grandChild = child1->AddChild(std::make_unique<SceneNode>("Grandchild"));

    EXPECT_EQ(root->FindDescendantById(root->id), root.get());
    EXPECT_EQ(root->FindDescendantById(child1->id), child1);
    EXPECT_EQ(root->FindDescendantById(child2->id), child2);
    EXPECT_EQ(root->FindDescendantById(grandChild->id), grandChild);
    EXPECT_EQ(child1->FindDescendantById(grandChild->id), grandChild);
    EXPECT_EQ(child2->FindDescendantById(grandChild->id), nullptr);
    EXPECT_EQ(root->FindDescendantById(999999u), nullptr);
}

TEST(SceneTreeOverhaulTest, SafeBatchDeletionDescendantFiltering) {
    auto root = std::make_shared<SceneNode>("Root");
    auto parent = root->AddChild(std::make_unique<SceneNode>("Parent"));
    auto child = parent->AddChild(std::make_unique<SceneNode>("Child"));
    auto grandChild = child->AddChild(std::make_unique<SceneNode>("Grandchild"));
    auto sibling = root->AddChild(std::make_unique<SceneNode>("Sibling"));

    std::vector<SceneNode*> toDelete = { parent, child, grandChild, sibling };

    // Filter out nodes whose ancestor is also being deleted
    toDelete.erase(std::remove_if(toDelete.begin(), toDelete.end(), [&](SceneNode* candidate) {
        for (auto* other : toDelete) {
            if (other != candidate && candidate->IsDescendantOf(other)) {
                return true;
            }
        }
        return false;
    }), toDelete.end());

    // Only top-level nodes (parent and sibling) should remain
    ASSERT_EQ(toDelete.size(), 2u);
    EXPECT_EQ(toDelete[0], parent);
    EXPECT_EQ(toDelete[1], sibling);
}


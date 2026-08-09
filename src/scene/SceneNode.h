#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include "../core/PropertyReflection.h"

#include "MeshComponent.h"

class SceneNode : public IReflectable {
public:
    SceneNode(const std::string& name = "Node");
    virtual ~SceneNode() = default;

    std::string name;
    uint32_t id;
    static uint32_t s_nextId;
    std::shared_ptr<MeshComponent> mesh;

    // Transform properties
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotationDegrees{0.0f, 0.0f, 0.0f}; // Euler angles for UI editing
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    bool lockScale = false; // Lock scale aspect ratio

    // Visibility — if false, node and all children are skipped during rendering
    bool visible = true;

    glm::mat4 GetLocalTransform() const;
    glm::mat4 GetWorldTransform() const;

    // Hierarchy
    SceneNode* GetParent() const { return m_parent; }
    const std::vector<std::unique_ptr<SceneNode>>& GetChildren() const { return m_children; }
    
    SceneNode* AddChild(std::unique_ptr<SceneNode> child);
    void RemoveChild(SceneNode* child);

    // Reflected Properties implementation
    std::vector<Property>& GetProperties() override { return m_properties; }
    const std::vector<Property>& GetProperties() const override { return m_properties; }

    void SyncPropertiesToTransform();
    void SyncTransformToProperties();

private:
    SceneNode* m_parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> m_children;
    std::vector<Property> m_properties;
};

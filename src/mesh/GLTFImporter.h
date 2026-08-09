#pragma once

#include <string>
#include <memory>
#include <vector>
#include "../scene/MeshComponent.h"
#include "../scene/SceneNode.h"
#include "../vulkan/VulkanContext.h"

#include "../vulkan/Descriptors.h"

class GLTFImporter {
public:
    // Loads a glTF (.gltf or .glb) file into a SceneNode containing MeshComponent primitives
    static std::shared_ptr<SceneNode> LoadFromFile(VulkanContext& context, const std::string& filepath,
                                                   VkDescriptorSetLayout setLayout = VK_NULL_HANDLE,
                                                   DescriptorAllocator* allocator = nullptr);

    // Creates sample primitive meshes for instant loading
    static std::shared_ptr<MeshComponent> CreateSampleMesh(VulkanContext& context, const std::string& primitiveName);
};

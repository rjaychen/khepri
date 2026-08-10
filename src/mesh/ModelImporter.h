#pragma once

#include <string>
#include <memory>
#include <vector>
#include "../scene/MeshComponent.h"
#include "../scene/SceneNode.h"
#include "../vulkan/VulkanContext.h"
#include "../vulkan/Descriptors.h"

// Base Abstract Importer for 3D Geometry and Model Formats
class ModelImporter {
public:
    virtual ~ModelImporter() = default;

    [[nodiscard]] virtual bool CanImport(const std::string& filepath) const = 0;
    [[nodiscard]] virtual std::shared_ptr<SceneNode> Import(VulkanContext& context, const std::string& filepath,
                                                            VkDescriptorSetLayout setLayout = VK_NULL_HANDLE,
                                                            DescriptorAllocator* allocator = nullptr,
                                                            VkBuffer lightUBOBuffer = VK_NULL_HANDLE) = 0;

    // Static Dispatcher Facade
    static std::shared_ptr<SceneNode> LoadFromFile(VulkanContext& context, const std::string& filepath,
                                                   VkDescriptorSetLayout setLayout = VK_NULL_HANDLE,
                                                   DescriptorAllocator* allocator = nullptr,
                                                   VkBuffer lightUBOBuffer = VK_NULL_HANDLE);

    static std::shared_ptr<MeshComponent> CreateSampleMesh(VulkanContext& context, const std::string& primitiveName);
};

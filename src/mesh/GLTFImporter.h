#pragma once

#include "ModelImporter.h"

class GLTFImporter : public ModelImporter {
public:
    GLTFImporter() = default;
    ~GLTFImporter() override = default;

    [[nodiscard]] bool CanImport(const std::string& filepath) const override;
    [[nodiscard]] std::shared_ptr<SceneNode> Import(VulkanContext& context, const std::string& filepath,
                                                            VkDescriptorSetLayout setLayout = VK_NULL_HANDLE,
                                                            DescriptorAllocator* allocator = nullptr,
                                                            VkBuffer lightUBOBuffer = VK_NULL_HANDLE) override;

    // Static facade methods maintained for backward compatibility
    static std::shared_ptr<SceneNode> LoadFromFile(VulkanContext& context, const std::string& filepath,
                                                   VkDescriptorSetLayout setLayout = VK_NULL_HANDLE,
                                                   DescriptorAllocator* allocator = nullptr,
                                                   VkBuffer lightUBOBuffer = VK_NULL_HANDLE) {
        return ModelImporter::LoadFromFile(context, filepath, setLayout, allocator, lightUBOBuffer);
    }

    static std::shared_ptr<MeshComponent> CreateSampleMesh(VulkanContext& context, const std::string& primitiveName) {
        return ModelImporter::CreateSampleMesh(context, primitiveName);
    }
};

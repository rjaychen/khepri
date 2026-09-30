#pragma once

#include "ModelImporter.h"

class GLTFImporter : public ModelImporter {
public:
    GLTFImporter() = default;
    ~GLTFImporter() override = default;

    [[nodiscard]] bool CanImport(const std::string& filepath) const override;
    [[nodiscard]] std::expected<std::shared_ptr<SceneNode>, khepri::ImportError> Import(
        VulkanContext& context, const std::string& filepath,
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE,
        DescriptorAllocator* allocator = nullptr) override;

    // Static facade methods maintained for backward compatibility
    static std::expected<std::shared_ptr<SceneNode>, khepri::ImportError> LoadFromFile(
        VulkanContext& context, const std::string& filepath,
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE,
        DescriptorAllocator* allocator = nullptr) {
        return ModelImporter::LoadFromFile(context, filepath, setLayout, allocator);
    }

    static std::shared_ptr<MeshComponent> CreateSampleMesh(VulkanContext& context, const std::string& primitiveName) {
        return ModelImporter::CreateSampleMesh(context, primitiveName);
    }
};

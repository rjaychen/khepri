#pragma once

#include "ModelImporter.h"

// Dedicated Wavefront OBJ (.obj) 3D Geometry Importer
class OBJImporter : public ModelImporter {
public:
    OBJImporter() = default;
    ~OBJImporter() override = default;

    [[nodiscard]] bool CanImport(const std::string& filepath) const override;
    [[nodiscard]] std::expected<std::shared_ptr<SceneNode>, khepri::ImportError> Import(
        VulkanContext& context, const std::string& filepath,
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE,
        DescriptorAllocator* allocator = nullptr,
        VkBuffer lightUBOBuffer = VK_NULL_HANDLE) override;
};

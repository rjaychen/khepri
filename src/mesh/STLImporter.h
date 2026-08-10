#pragma once

#include "ModelImporter.h"

// Dedicated STL (.stl) Binary and ASCII 3D Geometry Importer
class STLImporter : public ModelImporter {
public:
    STLImporter() = default;
    ~STLImporter() override = default;

    [[nodiscard]] bool CanImport(const std::string& filepath) const override;
    [[nodiscard]] std::shared_ptr<SceneNode> Import(VulkanContext& context, const std::string& filepath,
                                                            VkDescriptorSetLayout setLayout = VK_NULL_HANDLE,
                                                            DescriptorAllocator* allocator = nullptr,
                                                            VkBuffer lightUBOBuffer = VK_NULL_HANDLE) override;
};

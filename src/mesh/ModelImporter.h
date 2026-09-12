#pragma once

#include <string>
#include <memory>
#include <vector>
#include <expected>
#include <functional>
#include "../core/KhepriError.h"
#include "../scene/MeshComponent.h"
#include "../scene/SceneNode.h"
#include "../vulkan/VulkanContext.h"
#include "../vulkan/Descriptors.h"

// Base Abstract Importer for 3D Geometry and Model Formats
class ModelImporter {
public:
    virtual ~ModelImporter() = default;

    [[nodiscard]] virtual bool CanImport(const std::string& filepath) const = 0;
    [[nodiscard]] virtual std::expected<std::shared_ptr<SceneNode>, khepri::ImportError> Import(
        VulkanContext& context, const std::string& filepath,
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE,
        DescriptorAllocator* allocator = nullptr) = 0;

    // Static Dispatcher Facade
    [[nodiscard]] static std::expected<std::shared_ptr<SceneNode>, khepri::ImportError> LoadFromFile(
        VulkanContext& context, const std::string& filepath,
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE,
        DescriptorAllocator* allocator = nullptr);

    static std::shared_ptr<MeshComponent> CreateSampleMesh(VulkanContext& context, const std::string& primitiveName);

    struct ImporterRegistryEntry {
        std::vector<std::string> extensions;
        std::function<std::unique_ptr<ModelImporter>()> factory;
    };

    static void RegisterImporter(std::vector<std::string> extensions, std::function<std::unique_ptr<ModelImporter>()> factory);
    static const std::vector<ImporterRegistryEntry>& GetRegistry();
};


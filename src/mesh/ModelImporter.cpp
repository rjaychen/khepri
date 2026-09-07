#include "ModelImporter.h"
#include "GLTFImporter.h"
#include "OBJImporter.h"
#include "STLImporter.h"
#include "../core/Logger.h"
#include <filesystem>
#include <algorithm>

namespace {
std::vector<ModelImporter::ImporterRegistryEntry>& GetInternalRegistry() {
    static std::vector<ModelImporter::ImporterRegistryEntry> registry = {
        { {".gltf", ".glb"}, []() -> std::unique_ptr<ModelImporter> { return std::make_unique<GLTFImporter>(); } },
        { {".obj"},          []() -> std::unique_ptr<ModelImporter> { return std::make_unique<OBJImporter>(); } },
        { {".stl"},          []() -> std::unique_ptr<ModelImporter> { return std::make_unique<STLImporter>(); } }
    };
    return registry;
}
} // namespace

void ModelImporter::RegisterImporter(std::vector<std::string> extensions, std::function<std::unique_ptr<ModelImporter>()> factory) {
    GetInternalRegistry().push_back({ std::move(extensions), std::move(factory) });
}

const std::vector<ModelImporter::ImporterRegistryEntry>& ModelImporter::GetRegistry() {
    return GetInternalRegistry();
}

std::expected<std::shared_ptr<SceneNode>, khepri::ImportError> ModelImporter::LoadFromFile(
    VulkanContext& context, const std::string& filepath,
    VkDescriptorSetLayout setLayout, DescriptorAllocator* allocator, VkBuffer lightUBOBuffer) {

    // Resolve path check (handling relative search fallbacks)
    std::string resolvedPath = filepath;
    if (!std::filesystem::exists(resolvedPath)) {
        std::vector<std::string> candidates = {
            filepath,
            "../" + filepath,
            "../../" + filepath,
            "../../../" + filepath
        };
        for (const auto& candidate : candidates) {
            if (std::filesystem::exists(candidate)) {
                resolvedPath = candidate;
                break;
            }
        }
    }

    if (!std::filesystem::exists(resolvedPath)) {
        LOG_WARN("File not found for model import: " + filepath);
        return std::unexpected(khepri::ImportError::FileNotFound);
    }

    std::string ext = std::filesystem::path(resolvedPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });

    for (const auto& entry : GetInternalRegistry()) {
        for (const auto& supportedExt : entry.extensions) {
            std::string sExt = supportedExt;
            std::transform(sExt.begin(), sExt.end(), sExt.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
            if (sExt == ext) {
                auto importer = entry.factory();
                if (importer && importer->CanImport(resolvedPath)) {
                    return importer->Import(context, resolvedPath, setLayout, allocator, lightUBOBuffer);
                }
            }
        }
    }

    LOG_ERROR("Unsupported 3D model format or unrecognized file extension for: " + filepath);
    return std::unexpected(khepri::ImportError::UnsupportedFormat);
}

std::shared_ptr<MeshComponent> ModelImporter::CreateSampleMesh(VulkanContext& context, const std::string& primitiveName) {
    if (primitiveName == "Sphere") {
        return MeshComponent::CreateSphere(context, 1.2f, 32, 16);
    } else if (primitiveName == "Cylinder") {
        return MeshComponent::CreateCylinder(context, 0.8f, 2.0f, 32);
    } else if (primitiveName == "Plane") {
        return MeshComponent::CreatePlane(context, 4.0f, 8);
    }
    return MeshComponent::CreateCube(context, 2.0f);
}


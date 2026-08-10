#include "ModelImporter.h"
#include "GLTFImporter.h"
#include "OBJImporter.h"
#include "STLImporter.h"
#include "../core/Logger.h"
#include <algorithm>

std::shared_ptr<SceneNode> ModelImporter::LoadFromFile(VulkanContext& context, const std::string& filepath,
                                                       VkDescriptorSetLayout setLayout, DescriptorAllocator* allocator, VkBuffer lightUBOBuffer) {
    static GLTFImporter gltfImporter;
    static OBJImporter objImporter;
    static STLImporter stlImporter;

    static std::vector<ModelImporter*> importers = { &gltfImporter, &objImporter, &stlImporter };

    for (auto* importer : importers) {
        if (importer->CanImport(filepath)) {
            return importer->Import(context, filepath, setLayout, allocator, lightUBOBuffer);
        }
    }

    LOG_ERROR("Unsupported 3D model format or unrecognized file extension for: " + filepath);
    return nullptr;
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

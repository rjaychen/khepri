#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "GLTFImporter.h"
#include "../core/Logger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

bool GLTFImporter::CanImport(const std::string& filepath) const {
    size_t dotPos = filepath.find_last_of('.');
    if (dotPos == std::string::npos) return false;
    std::string ext = filepath.substr(dotPos);
    for (char& c : ext) c = static_cast<char>(tolower(c));
    return ext == ".gltf" || ext == ".glb";
}

std::shared_ptr<SceneNode> GLTFImporter::Import(VulkanContext& context, const std::string& filepath,
                                                 VkDescriptorSetLayout setLayout, DescriptorAllocator* allocator, VkBuffer lightUBOBuffer) {
    LOG_INFO("Loading glTF model via GLTFImporter: " + filepath);

    cgltf_options options = {};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filepath.c_str(), &data);

    if (result != cgltf_result_success) {
        LOG_ERROR("Failed to parse glTF file: " + filepath + " (Error code: " + std::to_string(result) + ")");
        return nullptr;
    }

    result = cgltf_load_buffers(&options, data, filepath.c_str());
    if (result != cgltf_result_success) {
        LOG_ERROR("Failed to load glTF buffers for: " + filepath
                  + " (cgltf error: " + std::to_string(static_cast<int>(result)) + ")");
        cgltf_free(data);
        return nullptr;
    }

    auto rootNode = std::make_shared<SceneNode>("glTF Root: " + filepath);
    uint32_t meshCount = 0;

    for (cgltf_size i = 0; i < data->meshes_count; ++i) {
        const cgltf_mesh& mesh = data->meshes[i];
        
        for (cgltf_size p = 0; p < mesh.primitives_count; ++p) {
            const cgltf_primitive& prim = mesh.primitives[p];
            if (prim.type != cgltf_primitive_type_triangles) continue;

            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            // 1. Read Positions
            const cgltf_accessor* posAccessor = nullptr;
            const cgltf_accessor* normAccessor = nullptr;
            const cgltf_accessor* uvAccessor = nullptr;

            for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
                if (prim.attributes[a].type == cgltf_attribute_type_position) {
                    posAccessor = prim.attributes[a].data;
                } else if (prim.attributes[a].type == cgltf_attribute_type_normal) {
                    normAccessor = prim.attributes[a].data;
                } else if (prim.attributes[a].type == cgltf_attribute_type_texcoord) {
                    uvAccessor = prim.attributes[a].data;
                }
            }

            if (!posAccessor) continue;
            vertices.resize(posAccessor->count);

            for (cgltf_size v = 0; v < posAccessor->count; ++v) {
                cgltf_accessor_read_float(posAccessor, v, &vertices[v].position.x, 3);
                if (normAccessor) cgltf_accessor_read_float(normAccessor, v, &vertices[v].normal.x, 3);
                if (uvAccessor)   cgltf_accessor_read_float(uvAccessor, v, &vertices[v].uv.x, 2);
            }

            // 2. Read Indices
            if (prim.indices) {
                indices.resize(prim.indices->count);
                for (cgltf_size idx = 0; idx < prim.indices->count; ++idx) {
                    indices[idx] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, idx));
                }
            } else {
                // Non-indexed geometry
                indices.resize(posAccessor->count);
                for (uint32_t idx = 0; idx < posAccessor->count; ++idx) {
                    indices[idx] = idx;
                }
            }

            // 3. Read Material Properties & Base Color Texture
            glm::vec4 baseColorFactor(1.0f);
            std::shared_ptr<Texture> primitiveTexture = nullptr;

            if (prim.material) {
                if (prim.material->has_pbr_metallic_roughness) {
                    const auto& pbr = prim.material->pbr_metallic_roughness;
                    baseColorFactor = glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1],
                                               pbr.base_color_factor[2], pbr.base_color_factor[3]);

                    if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image) {
                        const cgltf_image* image = pbr.base_color_texture.texture->image;
                        if (setLayout != VK_NULL_HANDLE && allocator) {
                            if (image->buffer_view) {
                                const uint8_t* imgData = reinterpret_cast<const uint8_t*>(image->buffer_view->buffer->data)
                                                       + image->buffer_view->offset;
                                size_t imgSize = image->buffer_view->size;
                                primitiveTexture = Texture::CreateFromMemory(context, imgData, imgSize, setLayout, *allocator, lightUBOBuffer);
                            } else if (image->uri) {
                                std::string uriStr = image->uri;
                                std::string dir = "";
                                size_t lastSlash = filepath.find_last_of("/\\");
                                if (lastSlash != std::string::npos) {
                                    dir = filepath.substr(0, lastSlash + 1);
                                }
                                std::string fullImagePath = dir + uriStr;
                                primitiveTexture = Texture::CreateFromFile(context, fullImagePath, setLayout, *allocator, lightUBOBuffer);
                            }
                        }
                    }
                }
            }

            std::string nodeName = (mesh.name && strlen(mesh.name) > 0) ? mesh.name : ("SubMesh_" + std::to_string(meshCount));
            auto meshNode = std::make_unique<SceneNode>(nodeName);
            auto meshComponent = std::make_shared<MeshComponent>(context, vertices, indices);
            meshComponent->SetBaseColorFactor(baseColorFactor);
            if (primitiveTexture) {
                meshComponent->SetTexture(primitiveTexture);
            }
            meshNode->mesh = meshComponent;
            rootNode->AddChild(std::move(meshNode));
            meshCount++;
        }
    }

    cgltf_free(data);

    LOG_INFO("Successfully imported glTF model: " + std::to_string(meshCount) + " primitive mesh nodes loaded.");
    return rootNode;
}

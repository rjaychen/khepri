#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "GLTFImporter.h"
#include "../core/Logger.h"
#include <iostream>

std::shared_ptr<SceneNode> GLTFImporter::LoadFromFile(VulkanContext& context, const std::string& filepath,
                                               VkDescriptorSetLayout setLayout, DescriptorAllocator* allocator) {
    LOG_INFO("Loading glTF model from: " + filepath);

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

            cgltf_size vertexCount = posAccessor->count;
            vertices.resize(vertexCount);

            for (cgltf_size v = 0; v < vertexCount; ++v) {
                float pos[3] = {0, 0, 0};
                cgltf_accessor_read_float(posAccessor, v, pos, 3);
                vertices[v].position = glm::vec3(pos[0], pos[1], pos[2]);

                if (normAccessor) {
                    float norm[3] = {0, 1, 0};
                    cgltf_accessor_read_float(normAccessor, v, norm, 3);
                    vertices[v].normal = glm::vec3(norm[0], norm[1], norm[2]);
                } else {
                    vertices[v].normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                if (uvAccessor) {
                    float uv[2] = {0, 0};
                    cgltf_accessor_read_float(uvAccessor, v, uv, 2);
                    vertices[v].uv = glm::vec2(uv[0], uv[1]);
                } else {
                    vertices[v].uv = glm::vec2(0.0f, 0.0f);
                }
            }

            // 2. Read Indices
            if (prim.indices) {
                cgltf_size indexCount = prim.indices->count;
                indices.resize(indexCount);
                for (cgltf_size idx = 0; idx < indexCount; ++idx) {
                    indices[idx] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, idx));
                }
            } else {
                // Non-indexed geometry
                indices.resize(vertexCount);
                for (uint32_t idx = 0; idx < vertexCount; ++idx) {
                    indices[idx] = idx;
                }
            }

            // 3. Read Material & Texture
            std::shared_ptr<Texture> primitiveTexture = nullptr;
            glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};

            if (prim.material) {
                const cgltf_material* mat = prim.material;
                if (mat->has_pbr_metallic_roughness) {
                    baseColorFactor = glm::vec4(
                        mat->pbr_metallic_roughness.base_color_factor[0],
                        mat->pbr_metallic_roughness.base_color_factor[1],
                        mat->pbr_metallic_roughness.base_color_factor[2],
                        mat->pbr_metallic_roughness.base_color_factor[3]
                    );

                    if (setLayout != VK_NULL_HANDLE && allocator != nullptr) {
                        const cgltf_texture_view& texView = mat->pbr_metallic_roughness.base_color_texture;
                        if (texView.texture && texView.texture->image) {
                            const cgltf_image* image = texView.texture->image;
                            if (image->buffer_view) {
                                const uint8_t* imgData = reinterpret_cast<const uint8_t*>(image->buffer_view->buffer->data) + image->buffer_view->offset;
                                size_t imgSize = image->buffer_view->size;
                                primitiveTexture = Texture::CreateFromMemory(context, imgData, imgSize, setLayout, *allocator);
                            } else if (image->uri) {
                                std::string uriStr = image->uri;
                                std::string dir = "";
                                size_t lastSlash = filepath.find_last_of("/\\");
                                if (lastSlash != std::string::npos) {
                                    dir = filepath.substr(0, lastSlash + 1);
                                }
                                std::string fullImagePath = dir + uriStr;
                                primitiveTexture = Texture::CreateFromFile(context, fullImagePath, setLayout, *allocator);
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

std::shared_ptr<MeshComponent> GLTFImporter::CreateSampleMesh(VulkanContext& context, const std::string& primitiveName) {
    if (primitiveName == "Sphere") {
        return MeshComponent::CreateSphere(context, 1.2f, 32, 16);
    } else if (primitiveName == "Cylinder") {
        return MeshComponent::CreateCylinder(context, 0.8f, 2.0f, 32);
    } else if (primitiveName == "Plane") {
        return MeshComponent::CreatePlane(context, 5.0f, 10);
    }
    // Default to Cube
    return MeshComponent::CreateCube(context, 2.0f);
}

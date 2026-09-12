#include "STLImporter.h"
#include "../core/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

bool STLImporter::CanImport(const std::string& filepath) const {
    size_t dotPos = filepath.find_last_of('.');
    if (dotPos == std::string::npos) return false;
    std::string ext = filepath.substr(dotPos);
    for (char& c : ext) c = static_cast<char>(tolower(c));
    return ext == ".stl";
}

std::expected<std::shared_ptr<SceneNode>, khepri::ImportError> STLImporter::Import(VulkanContext& context, const std::string& filepath,
                                                VkDescriptorSetLayout, DescriptorAllocator*) {
    LOG_INFO("Loading STL model via STLImporter: " + filepath);
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open STL file: " + filepath);
        return std::unexpected(khepri::ImportError::FileNotFound);
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    char header[80];
    file.read(header, 80);
    uint32_t triangleCount = 0;
    file.read(reinterpret_cast<char*>(&triangleCount), sizeof(uint32_t));

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();

    bool isBinary = (fileSize == 80 + 4 + static_cast<size_t>(triangleCount) * 50);

    if (isBinary) {
        file.seekg(84, std::ios::beg);
        for (uint32_t i = 0; i < triangleCount; ++i) {
            float n[3], v0[3], v1[3], v2[3];
            uint16_t attrByteCount;
            file.read(reinterpret_cast<char*>(n), 12);
            file.read(reinterpret_cast<char*>(v0), 12);
            file.read(reinterpret_cast<char*>(v1), 12);
            file.read(reinterpret_cast<char*>(v2), 12);
            file.read(reinterpret_cast<char*>(&attrByteCount), 2);

            glm::vec3 normal(n[0], n[1], n[2]);
            if (glm::length(normal) < 1e-5f) {
                glm::vec3 e1 = glm::vec3(v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]);
                glm::vec3 e2 = glm::vec3(v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]);
                normal = glm::cross(e1, e2);
                if (glm::length(normal) > 1e-5f) normal = glm::normalize(normal);
            }

            uint32_t baseIdx = static_cast<uint32_t>(vertices.size());
            vertices.push_back(Vertex{
                .position = glm::vec3(v0[0], v0[1], v0[2]),
                .normal = normal,
                .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                .uv = glm::vec2(0.0f),
                .jointIndices = glm::uvec4(0),
                .jointWeights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)
            });
            vertices.push_back(Vertex{
                .position = glm::vec3(v1[0], v1[1], v1[2]),
                .normal = normal,
                .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                .uv = glm::vec2(1.0f, 0.0f),
                .jointIndices = glm::uvec4(0),
                .jointWeights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)
            });
            vertices.push_back(Vertex{
                .position = glm::vec3(v2[0], v2[1], v2[2]),
                .normal = normal,
                .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                .uv = glm::vec2(0.5f, 1.0f),
                .jointIndices = glm::uvec4(0),
                .jointWeights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)
            });

            indices.push_back(baseIdx);
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 2);
        }
    } else {
        file.close();
        std::ifstream asciiFile(filepath);
        std::string line;
        glm::vec3 currentNorm(0.0f, 1.0f, 0.0f);
        std::vector<glm::vec3> faceVerts;

        while (std::getline(asciiFile, line)) {
            std::istringstream ss(line);
            std::string word;
            ss >> word;
            if (word == "facet") {
                std::string normalWord;
                ss >> normalWord >> currentNorm.x >> currentNorm.y >> currentNorm.z;
                faceVerts.clear();
            } else if (word == "vertex") {
                glm::vec3 pos;
                ss >> pos.x >> pos.y >> pos.z;
                faceVerts.push_back(pos);
            } else if (word == "endfacet") {
                if (faceVerts.size() == 3) {
                    uint32_t baseIdx = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(Vertex{
                        .position = faceVerts[0],
                        .normal = currentNorm,
                        .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                        .uv = glm::vec2(0.0f),
                        .jointIndices = glm::uvec4(0),
                        .jointWeights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)
                    });
                    vertices.push_back(Vertex{
                        .position = faceVerts[1],
                        .normal = currentNorm,
                        .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                        .uv = glm::vec2(1.0f, 0.0f),
                        .jointIndices = glm::uvec4(0),
                        .jointWeights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)
                    });
                    vertices.push_back(Vertex{
                        .position = faceVerts[2],
                        .normal = currentNorm,
                        .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                        .uv = glm::vec2(0.5f, 1.0f),
                        .jointIndices = glm::uvec4(0),
                        .jointWeights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)
                    });
                    indices.push_back(baseIdx);
                    indices.push_back(baseIdx + 1);
                    indices.push_back(baseIdx + 2);
                }
            }
        }
    }

    if (vertices.empty() || indices.empty()) {
        LOG_ERROR("Failed to parse valid STL geometry from: " + filepath);
        return std::unexpected(khepri::ImportError::ParsingFailed);
    }

    auto meshComp = std::make_shared<MeshComponent>(context, vertices, indices);
    auto rootNode = std::make_shared<SceneNode>("STL Model: " + filepath);
    auto childNode = std::make_unique<SceneNode>("STL Mesh");
    childNode->mesh = meshComp;
    rootNode->AddChild(std::move(childNode));

    LOG_INFO("Successfully imported STL model (" + std::to_string(vertices.size()) + " vertices, " + std::to_string(indices.size() / 3) + " triangles)");
    return rootNode;
}

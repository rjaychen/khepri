#include "OBJImporter.h"
#include "../core/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <filesystem>

bool OBJImporter::CanImport(const std::string& filepath) const {
    size_t dotPos = filepath.find_last_of('.');
    if (dotPos == std::string::npos) return false;
    std::string ext = filepath.substr(dotPos);
    for (char& c : ext) c = static_cast<char>(tolower(c));
    return ext == ".obj";
}

std::expected<std::shared_ptr<SceneNode>, khepri::ImportError> OBJImporter::Import(VulkanContext& context, const std::string& filepath,
                                                VkDescriptorSetLayout, DescriptorAllocator*, VkBuffer) {
    LOG_INFO("Loading Wavefront OBJ model via OBJImporter: " + filepath);

    std::string resolvedPath = filepath;
    if (!std::filesystem::exists(resolvedPath)) {
        std::vector<std::string> candidates = {
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

    std::ifstream file(resolvedPath);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open OBJ file: " + resolvedPath);
        return std::unexpected(khepri::ImportError::FileNotFound);
    }

    std::vector<glm::vec3> temp_positions;
    std::vector<glm::vec3> temp_normals;
    std::vector<glm::vec2> temp_uvs;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<std::string, uint32_t> uniqueVertices;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            glm::vec3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            temp_positions.push_back(pos);
        } else if (prefix == "vn") {
            glm::vec3 norm;
            ss >> norm.x >> norm.y >> norm.z;
            temp_normals.push_back(norm);
        } else if (prefix == "vt") {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            temp_uvs.push_back(uv);
        } else if (prefix == "f") {
            std::vector<std::string> faceTokens;
            std::string token;
            while (ss >> token) {
                faceTokens.push_back(token);
            }

            if (faceTokens.size() < 3) continue;

            auto parseIndexToken = [&](const std::string& tok) -> uint32_t {
                auto it = uniqueVertices.find(tok);
                if (it != uniqueVertices.end()) {
                    return it->second;
                }

                int vIdx = 0, vtIdx = 0, vnIdx = 0;
                size_t firstSlash = tok.find('/');
                if (firstSlash == std::string::npos) {
                    vIdx = std::stoi(tok);
                } else {
                    vIdx = std::stoi(tok.substr(0, firstSlash));
                    size_t secondSlash = tok.find('/', firstSlash + 1);
                    if (secondSlash == std::string::npos) {
                        vtIdx = std::stoi(tok.substr(firstSlash + 1));
                    } else {
                        if (secondSlash > firstSlash + 1) {
                            vtIdx = std::stoi(tok.substr(firstSlash + 1, secondSlash - firstSlash - 1));
                        }
                        if (secondSlash + 1 < tok.size()) {
                            vnIdx = std::stoi(tok.substr(secondSlash + 1));
                        }
                    }
                }

                Vertex vert{};
                if (vIdx > 0 && static_cast<size_t>(vIdx) <= temp_positions.size()) {
                    vert.position = temp_positions[vIdx - 1];
                } else if (vIdx < 0 && static_cast<int>(temp_positions.size()) + vIdx >= 0) {
                    vert.position = temp_positions[temp_positions.size() + vIdx];
                }

                if (vnIdx > 0 && static_cast<size_t>(vnIdx) <= temp_normals.size()) {
                    vert.normal = temp_normals[vnIdx - 1];
                } else if (vnIdx < 0 && static_cast<int>(temp_normals.size()) + vnIdx >= 0) {
                    vert.normal = temp_normals[temp_normals.size() + vnIdx];
                }

                if (vtIdx > 0 && static_cast<size_t>(vtIdx) <= temp_uvs.size()) {
                    vert.uv = temp_uvs[vtIdx - 1];
                } else if (vtIdx < 0 && static_cast<int>(temp_uvs.size()) + vtIdx >= 0) {
                    vert.uv = temp_uvs[temp_uvs.size() + vtIdx];
                }

                uint32_t newIdx = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vert);
                uniqueVertices[tok] = newIdx;
                return newIdx;
            };

            // Triangulate polygon face
            uint32_t idx0 = parseIndexToken(faceTokens[0]);
            for (size_t i = 1; i + 1 < faceTokens.size(); ++i) {
                uint32_t idx1 = parseIndexToken(faceTokens[i]);
                uint32_t idx2 = parseIndexToken(faceTokens[i + 1]);
                indices.push_back(idx0);
                indices.push_back(idx1);
                indices.push_back(idx2);
            }
        }
    }

    if (vertices.empty() || indices.empty()) {
        LOG_ERROR("Failed to parse valid OBJ geometry from: " + filepath);
        return std::unexpected(khepri::ImportError::ParsingFailed);
    }

    if (temp_normals.empty()) {
        for (size_t i = 0; i < indices.size(); i += 3) {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];
            glm::vec3 edge1 = vertices[i1].position - vertices[i0].position;
            glm::vec3 edge2 = vertices[i2].position - vertices[i0].position;
            glm::vec3 norm = glm::cross(edge1, edge2);
            if (glm::length(norm) > 1e-5f) norm = glm::normalize(norm);
            vertices[i0].normal += norm;
            vertices[i1].normal += norm;
            vertices[i2].normal += norm;
        }
        for (auto& v : vertices) {
            if (glm::length(v.normal) > 1e-5f) v.normal = glm::normalize(v.normal);
            else v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    auto meshComp = std::make_shared<MeshComponent>(context, vertices, indices);
    auto rootNode = std::make_shared<SceneNode>("OBJ Model: " + filepath);
    auto childNode = std::make_unique<SceneNode>("OBJ Mesh");
    childNode->mesh = meshComp;
    rootNode->AddChild(std::move(childNode));

    LOG_INFO("Successfully imported OBJ model (" + std::to_string(vertices.size()) + " vertices, " + std::to_string(indices.size() / 3) + " triangles)");
    return rootNode;
}

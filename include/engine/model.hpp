#pragma once

#include <engine/handle.hpp>

#include <string>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>

namespace eng {

enum class TextureType : uint8_t{
    None, Diffuse, Normal
};

struct MeshMaterial {
    std::string shader_name;
    std::unordered_map<TextureType, std::string> texture_paths;
};

struct Mesh {
    MeshMaterial material; // I think this should be separated
    std::vector<uint32_t> vertex_indices;
    std::vector<glm::vec3> vertex_positions;
    std::vector<glm::vec3> vertex_normals;
    std::vector<glm::vec2> vertex_texture_coords;
};

struct Geometry : public Handle<Geometry> {
    Geometry() : Handle(GENERATE_HANDLE) { }
    explicit Geometry(const std::vector<Mesh>& meshes): Handle(HandleGenerator<Geometry>::generate()), meshes(meshes) { }
    explicit Geometry(std::vector<Mesh>&& meshes): Handle(HandleGenerator<Geometry>::generate()), meshes(std::move(meshes)) { }

    std::vector<Mesh> meshes;
};

struct Model {
    // in the future, it may be neccessary to change this pointer
    // into something mutable for i.e. mesh morphing?
    const Geometry *geometry{};
};

}
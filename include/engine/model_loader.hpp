#pragma once

#include <engine/model.hpp>

#include <filesystem>

// assimp has faulty headers which 
// are the cause of compiler errors with -Werror
#pragma clang system_header
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

namespace eng {

class GeometryImporter {
public:
    static Geometry import(const std::filesystem::path &path);

private:
    GeometryImporter(const aiScene *scene, const std::string &base_path): scene(scene), base_path(base_path) {}

    void _parse_aiscene_nodes_rec(const aiNode *ai, Geometry &geometry);
    Mesh _parse_aimesh(const aiMesh *ai);

    const aiScene *scene{};
    std::string base_path;
};
    
}
#include <engine/model_loader.hpp>

namespace eng {

Geometry GeometryImporter::import(const std::filesystem::path &path) {
    if(!std::filesystem::is_regular_file(path)) { return Geometry{}; }
    if(!std::filesystem::exists(path)) { return Geometry{}; }

    Assimp::Importer aiimp;
    const auto scene = aiimp.ReadFile(path.string(), aiProcess_GenNormals | aiProcess_FlipUVs | aiProcess_GenBoundingBoxes | aiProcess_Triangulate);

    if(!scene) { return Geometry{}; }

    GeometryImporter imp{scene, path.parent_path().string()};
    Geometry geom;
    imp._parse_aiscene_nodes_rec(scene->mRootNode, geom);
    return geom;
}

void GeometryImporter::_parse_aiscene_nodes_rec(const aiNode *ai, Geometry &geometry) {
    for(auto i=0u; i<ai->mNumMeshes; ++i) {
        const auto *aim = scene->mMeshes[ai->mMeshes[i]];
        geometry.meshes.push_back(_parse_aimesh(aim));
    }

    for(auto i=0u; i<ai->mNumChildren; ++i) {
        _parse_aiscene_nodes_rec(ai->mChildren[i], geometry);
    }
}

Mesh GeometryImporter::_parse_aimesh(const aiMesh *ai) {
    Mesh mesh;

    if(!ai->HasPositions()) { return mesh; }

    mesh.vertex_positions.resize(ai->mNumVertices); 
    for(auto i=0u; i<ai->mNumVertices; ++i) {
        const auto &[x, y, z] = ai->mVertices[i];
        mesh.vertex_positions[i] = {x, y, z};
    }

    if(ai->HasNormals()) {
        mesh.vertex_normals.resize(ai->mNumVertices * 3); 
        for(auto i=0u; i<ai->mNumVertices; ++i) {
            const auto &[x, y, z] = ai->mNormals[i];
            mesh.vertex_normals[i] = {x, y, z};
        }
    }

    if(ai->HasTextureCoords(0)) {
        mesh.vertex_texture_coords.resize(ai->mNumVertices * 3); 
        for(auto i=0u; i<ai->mNumVertices; ++i) {
            const auto &[x, y, z] = ai->mTextureCoords[0][i];
            mesh.vertex_texture_coords[i] = {x, y};
        }
    }

    mesh.vertex_indices.resize(ai->mNumFaces * 3);
    for(auto i=0u; i<ai->mNumFaces; ++i) {
        const auto idx = i*3;
        mesh.vertex_indices[idx + 0] = ai->mFaces[i].mIndices[0];
        mesh.vertex_indices[idx + 1] = ai->mFaces[i].mIndices[1];
        mesh.vertex_indices[idx + 2] = ai->mFaces[i].mIndices[2];
    }
    
    if(scene->HasMaterials() && ai->mMaterialIndex < scene->mNumMaterials) {
        const auto *aimat = scene->mMaterials[ai->mMaterialIndex];

        aiString aipath;
        if(aimat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aimat->GetTexture(aiTextureType_DIFFUSE, 0, &aipath);
            mesh.material.texture_paths[TextureType::Diffuse] = (std::filesystem::path{base_path} / aipath.C_Str()).string();
        }
        if(aimat->GetTextureCount(aiTextureType_NORMALS) > 0) {
            aimat->GetTexture(aiTextureType_NORMALS, 0, &aipath);
            mesh.material.texture_paths[TextureType::Normal] = (std::filesystem::path{base_path} / aipath.C_Str()).string();
        }
    }

    return mesh;
}

}
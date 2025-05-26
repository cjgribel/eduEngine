// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "AssimpImporter.hpp"
#include "ResourceRegistry.hpp"
#include "ThreadPool.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>

namespace eeng {

struct AssimpImporter::Impl
{
    Assimp::Importer importer;

    const aiScene* load_scene(const std::string& file_path, unsigned int assimp_flags, std::string& error)
    {
        const aiScene* scene = importer.ReadFile(file_path, assimp_flags);
        if (!scene || !scene->mRootNode)
        {
            error = importer.GetErrorString();
            return nullptr;
        }
        return scene;
    }
};

AssimpImporter::AssimpImporter()
    : m_impl(std::make_unique<Impl>())
{
}

AssimpImporter::~AssimpImporter() = default;

ModelImportResult AssimpImporter::import_model(const std::string& file_path,
                                               const ModelImportOptions& options,
                                               ResourceRegistry& registry,
                                               ThreadPool* thread_pool)
{
    ModelImportResult result;
    result.source_path = file_path;

    // Translate flags
    unsigned int assimp_flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices;
    if (options.flags != ImportFlags::None)
    {
        if ((to_integral(options.flags) & to_integral(ImportFlags::GenerateTangents)) != 0)
            assimp_flags |= aiProcess_CalcTangentSpace;
        if ((to_integral(options.flags) & to_integral(ImportFlags::FlipUVs)) != 0)
            assimp_flags |= aiProcess_FlipUVs;
        if ((to_integral(options.flags) & to_integral(ImportFlags::OptimizeMesh)) != 0)
            assimp_flags |= aiProcess_ImproveCacheLocality;
    }

    std::string error;
    const aiScene* scene = m_impl->load_scene(file_path, assimp_flags, error);
    if (!scene)
    {
        result.error_message = error;
        return result;
    }

    std::cout << "[Importer] Loaded " << file_path << ": "
              << scene->mNumMeshes << " meshes, "
              << scene->mNumAnimations << " animations.\n";

    if (options.append_animations)
    {
        std::cout << "[Importer] Appending animations to model " << options.model.ofs << "\n";

        // TODO: extract_animation_data(scene, options.model, registry);
    }
    else
    {
        std::cout << "[Importer] Importing full model into handle " << options.model.ofs << "\n";

        // TODO:
        // 1. extract_meshes(scene, options.model, registry);
        // 2. extract_materials(scene, options.model, registry, thread_pool);
        // 3. extract_skeleton(scene, options.model, registry);
        // 4. extract_animations(scene, options.model, registry);
    }

    result.success = true;
    return result;
}

} // namespace eeng

// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef ASSIMP_IMPORTER_HPP
#define ASSIMP_IMPORTER_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "Handle.h"
#include "ThreadPool.hpp"
#include "ResourceRegistry.hpp"
#include "Texture.hpp"

// Placeholder resources
namespace eeng
{
    struct Mesh { size_t x; };
    struct Material { size_t x; };
    struct Skeleton { size_t x; };
    struct AnimationClip { size_t x; };

    using MeshHandle = Handle<Mesh>;
    using MaterialHandle = Handle<Material>;
    using TextureHandle = Handle<Texture2D>;
    using SkeletonHandle = Handle<Skeleton>;
    using AnimationClipHandle = Handle<AnimationClip>;

    struct Model
    {
        std::vector<Handle<Mesh>> meshes;
        std::vector<Handle<Texture2D>> textures;
        std::vector<Handle<Material>> materials;
        std::optional<Handle<Skeleton>> skeleton;
        std::vector<Handle<AnimationClip>> animation_clips;

        std::string source_file;
        uint32_t loading_flags;
    };
    using ModelHandle = Handle<Model>;


} // namespace eeng

namespace eeng
{
    class ResourceRegistry;
    class ThreadPool;

    enum class ImportFlags : unsigned int
    {
        None = 0,
        GenerateTangents = 1 << 0,
        FlipUVs = 1 << 1,
        OptimizeMesh = 1 << 2,
        // Extend as needed
    };

    struct ModelImportOptions
    {
        Handle<Model> model;                  // Valid handle, may refer to an empty model
        bool append_animations = false;      // If true, importer only loads animation data
        float scale = 1.0f;
        ImportFlags flags = ImportFlags::None;
    };

    struct ModelImportResult
    {
        bool success = false;
        std::string error_message;
        std::string source_path;
    };

    class AssimpImporter
    {
    public:
        AssimpImporter();
        ~AssimpImporter();

        /**
         * @brief Import a model or animation from a file.
         *
         * @param file_path Path to the FBX/OBJ/etc. file to import.
         * @param options Import options, including target model and mode.
         * @param registry Reference to the resource registry for creating/updating handles.
         * @param thread_pool Optional thread pool for parallel processing (e.g., textures).
         */
        ModelImportResult import_model(const std::string& file_path,
            const ModelImportOptions& options,
            ResourceRegistry& registry,
            ThreadPool* thread_pool = nullptr);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace eeng

#endif // ASSIMP_IMPORTER_HPP
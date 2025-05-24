// AssimpLoader.hpp
#pragma once

// #include <chrono>
// #include <iostream>
// #include <iostream>
// #include <ostream>
// #include <iomanip>
// #include <mutex>
// #include <string>
// #include <unordered_map>

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

    class AssimpImporter
    {
    public:
        static Handle<Model> load_into(const std::string& file,
            ResourceRegistry& registry,
            ThreadPool& pool);
    };
} // namespace eeng


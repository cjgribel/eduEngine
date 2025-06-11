// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

// ResourceRegistry.hpp
#pragma once
// #include <cstdint>
// #include <limits>
// #include <vector>
// #include <unordered_map>
// #include <typeindex>
// #include <memory>
// #include <string>

// #include <chrono>
// #include <iostream>
// #include <iostream>
// #include <ostream>
// #include <iomanip>
// #include <mutex>
// #include <string>
// #include <unordered_map>
// #include <optional>
// #include <unordered_map>

// #include "Handle.h"
// #include "Guid.h"
// #include "FreelistPool.h"
// #include "Texture.hpp"

namespace eeng
{
    class AssetDatabase 
    {
    public:
        // Initialization
        void initialize(const fs::path& meta_folder);

        // Asset Management (Add/Remove)
        template<typename T>
        Guid add_asset(const std::string& original_name, const T& asset_data, const ImportSettings& settings);

        bool remove_asset(const Guid& guid);

        // Querying Assets
        std::optional<Guid> find_guid_by_name(const std::string& asset_name, ResourceType type) const;
        std::vector<AssetEntry> list_assets(ResourceType type) const;

        // Loading/Deserialization
        template<typename T>
        std::optional<T> load_asset(const Guid& guid) const;

        // Batch operations (future concurrency support)
        template<typename T>
        std::vector<std::optional<T>> load_assets(const std::vector<Guid>& guids) const;
    };

#if 0
    class AssetDatabase {
        fs::path meta_root;

    public:
        AssetDatabase(const fs::path& project_path)
            : meta_root(project_path / "meta")
        {
        }

        template<typename T>
        Guid import_and_serialize(const std::string& original_name, const T& resource) {
            Guid guid = create_guid();
            fs::path type_folder = meta_root / resource_type_folder<T>();
            fs::create_directories(type_folder);

            fs::path resource_path = type_folder / (guid.to_string() + resource_extension<T>());
            fs::path meta_path = resource_path;
            meta_path += ".meta";

            serialize_to_disk(resource, resource_path);

            nlohmann::json meta_json = {
                {"guid", guid.to_string()},
                {"original_name", original_name}
            };
            std::ofstream meta_file(meta_path);
            meta_file << meta_json.dump(4);

            return guid;
        }

        template<typename T>
        std::optional<T> deserialize_resource(const Guid& guid) const {
            fs::path resource_path = meta_root / resource_type_folder<T>() / (guid.to_string() + resource_extension<T>());
            return deserialize_from_disk<T>(resource_path);
        }

    private:
        template<typename T>
        std::string resource_type_folder() const; // specialize per type

        template<typename T>
        std::string resource_extension() const;   // specialize per type

        Guid create_guid() const {
            std::random_device rd;
            std::mt19937_64 gen(rd());
            uint64_t high = gen(), low = gen();
            return Guid(high, low);
        }

        template<typename T>
        void serialize_to_disk(const T& resource, const fs::path& path) {
            std::ofstream ofs(path, std::ios::binary);
            resource.serialize(ofs);
        }

        template<typename T>
        std::optional<T> deserialize_from_disk(const fs::path& path) const {
            if (!fs::exists(path)) return std::nullopt;
            std::ifstream ifs(path, std::ios::binary);
            T resource;
            resource.deserialize(ifs);
            return resource;
        }
    };
#endif

} // namespace eeng
// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include <cstdint>
#include <limits>
#include <vector>
#include <unordered_map>
#include <type_traits> // for std::is_copy_constructible, std::is_move_constructible, etc.
#include <typeindex>
#include <memory>
#include <string>

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

#include "entt/entt.hpp"
#include "Handle.h"
#include "Guid.h"
#include "PoolAllocatorTFH.h"
// #include "Texture.hpp"

namespace eeng
{
    struct MetaHandle
    {
        handle_ofs_type ofs;
        handle_ver_type ver;
        entt::meta_type type = {};

        MetaHandle()
            : ofs(handle_ofs_null), ver(handle_ver_null) {
        }

        template<typename T>
        MetaHandle(Handle<T> h)
            : ofs(h.ofs), ver(h.ver), type(entt::resolve<T>()) {
        }

        bool valid() const noexcept {
            return ofs != handle_ofs_null
                && ver != handle_ver_null
                && static_cast<bool>(type);
        }

        bool empty() const {
            return !valid();
        }

        auto operator!() const noexcept { return !valid(); }
        explicit operator bool() const noexcept { return valid(); }

        template<typename T>
        std::optional<Handle<T>> cast() const
        {
            // ensure the run‐time type matches
            if (type != entt::resolve<T>()) {
                return std::nullopt;
            }
            return Handle<T>{ ofs, ver };
        }

        bool operator==(const MetaHandle& other) const {
            return ofs == other.ofs && ver == other.ver && type == other.type;
        }
    };
} // namespace eeng

namespace std {
    template<>
    struct hash<eeng::MetaHandle> 
    {
        size_t operator()(eeng::MetaHandle const& m) const noexcept 
        {
            return ::hash_combine(m.ofs, m.ver, m.type.id());
        }
    };
}

namespace eeng
{
    template<class T>
    class VersionMap
    {
        std::vector<handle_ver_type> versions;
        static constexpr size_t elem_size = sizeof(T);

    public:
        VersionMap() = default;

        void resize(size_t bytes)
        {
            versions.resize(bytes / elem_size, handle_ver_null);
        }

        // Assign version to handle, either 0 or current version
        void versionify(Handle<T>& handle)
        {
            assert(handle);
            auto index = get_index(handle);

            if (versions[index] == handle_ver_null)
                handle.ver = versions[index] = 0;
            else
                handle.ver = versions[index];
        }

        // Handle is valid if not null and version matches
        bool validate(const Handle<T>& handle) const
        {
            if (handle.ver == handle_ver_null) return false;

            auto index = get_index(handle);
            return handle.ver == versions[index];
        }

        // Increment version for handle, used when removing
        void remove(const Handle<T>& handle)
        {
            assert(handle);
            auto index = get_index(handle);
            versions[index]++;
        }

        void print() const
        {
            for (auto& v : versions)
                std::cout << v << ", ";
            std::cout << std::endl;
        }

    private:

        handle_ofs_type get_index(const Handle<T>& handle) const
        {
            auto index = handle.ofs / elem_size;
            assert(index < versions.size());
            return index;
        }
    };

    class Storage
    {
    public:
        Storage() = default;
        // Storage(const Storage&) = delete;
        // Storage& operator=(const Storage&) = delete;
        // Storage(Storage&&) = delete;
        // Storage& operator=(Storage&&) = delete;
        Storage(Storage const&) = delete;
        Storage& operator=(Storage const&) = delete;
        Storage(Storage&&) noexcept = default;
        Storage& operator=(Storage&& other) noexcept
        {
            pools.swap(other.pools);
            return *this;
        }

        template<typename T>
        void assure_storage()
        {
            auto type = entt::resolve<T>();
            auto id = type.id();

            if (pools.find(id) == pools.end())
                pools[id] = std::make_unique<Pool<T>>();
        }

        // NOT TEMPLATED
        // Return untyped handle
        // Takes an instance probably, not ... (Resource created elsewhere / by AssetIndex)
        // add(meta_type, meta_any, const Guid& guid = Guid::invalid())
        MetaHandle add(
            entt::meta_type type,
            entt::meta_any data,
            const Guid& guid)
        {
            // auto id = type.id();
            // auto it = pools.find(id);
            // if (it == pools.end())
            // {
            //     type.func("assure_storage"_hs).invoke({}, entt::forward_as_meta(*this));
            //     it = pools.find(id);
            //     if (it == pools.end())
            //         throw std::runtime_error("Pool creation failed");
            // }

            // auto& pool = static_cast<Pool<void>&>(*it->second);
            // pool.add(guid, data);
            return MetaHandle {};
        }


    private:
        class IPool
        {
        public:
            virtual ~IPool() = default;
        };

        template<typename T>
        class Pool : public IPool
        {
        public:
            using Handle = Handle<T>;
        private:
            PoolAllocatorTFH<T> m_pool;
            VersionMap<T> m_versions;
            std::vector<uint32_t> m_ref_counts;

            std::unordered_map<Guid, Handle> m_guid_map;
            std::unordered_map<Handle, Guid> m_handle_to_guid;

            mutable std::mutex m_mutex;
        };

        // -> registry.storage() -> [entt::id_type, entt::meta_type]
        // pool()

        // template<class T>
        // pool()

        // pool(entt::id_type id) 

    private:
        // IPool& get_or_create_pool(entt::meta_type type)
        // {
        //     auto id = type.id();
        //     auto it = pools.find(id);
        //     if (it == pools.end())
        //     {
        //         type.func("assure_storage"_hs).invoke({}, entt::forward_as_meta(*this));
        //         it = pools.find(id);
        //         if (it == pools.end())
        //             throw std::runtime_error("Pool creation failed");
        //     }
        //     return *it->second;
        // }

        mutable std::unordered_map<entt::id_type, std::unique_ptr<IPool>> pools;
    };

    static_assert(!std::is_copy_constructible_v<Storage>);
    static_assert(!std::is_copy_assignable_v<Storage>);
    static_assert(std::is_move_constructible_v<Storage>);
    static_assert(std::is_move_assignable_v<Storage>);

#if 0
#define version_null 0


    class IResourcePool
    {
    public:
        virtual ~IResourcePool() = default;
    };

    template<typename T>
    class ResourcePool : public IResourcePool
    {
    public:
        using Handle = Handle<T>;

        ResourcePool()
            : m_pool(TypeInfo::create<T>())
        {
        }

        template<typename... Args>
        Handle add(const Guid& guid, Args&&... args)
        {
            std::lock_guard lock(m_mutex);

            if (guid == Guid::invalid())
                throw std::runtime_error("Cannot add resource with invalid GUID");
            if (m_guid_map.find(guid) != m_guid_map.end())
                throw std::runtime_error("Resource with this GUID already exists");

            Handle h = m_pool.create<T>(std::forward<Args>(args)...);
            ensure_metadata(h);
            m_versions.versionify(h);
            m_ref_counts[h.ofs / sizeof(T)] = 1;

            m_guid_map[guid] = h;
            m_handle_to_guid[h] = guid;

            return h;
        }

        T& get(Handle h)
        {
            std::lock_guard lock(m_mutex);
            if (!m_versions.validate(h))
                throw std::runtime_error("Invalid handle (version mismatch)");
            return m_pool.get<T>(h);
        }

        const T& get(Handle h) const
        {
            std::lock_guard lock(m_mutex);
            if (!m_versions.validate(h))
                throw std::runtime_error("Invalid handle (version mismatch)");
            return m_pool.get<T>(h);
        }

        void remove(Handle h)
        {
            std::lock_guard lock(m_mutex);
            remove_unlocked(h);
        }

        template<typename F>
        void for_each(F&& f)
        {
            std::lock_guard lock(m_mutex);

            m_pool.template used_visitor<T>([&](T& obj)
                {
                    f(obj);
                });
        }

        void retain(Handle h)
        {
            std::lock_guard lock(m_mutex);
            if (!m_versions.validate(h)) return;
            ++m_ref_counts[h.ofs / sizeof(T)];
        }

        void release(Handle h)
        {
            std::lock_guard lock(m_mutex);
            if (!m_versions.validate(h)) return;

            auto& count = m_ref_counts[h.ofs / sizeof(T)];
            if (--count == 0)
            {
                remove_unlocked(h); // safe: avoids re-locking
            }
        }

        uint32_t use_count(Handle h) const
        {
            std::lock_guard lock(m_mutex);
            if (!m_versions.validate(h)) return 0;
            return m_ref_counts[h.ofs / sizeof(T)];
        }

        bool valid(Handle h) const
        {
            std::lock_guard lock(m_mutex);
            return m_versions.validate(h);
        }

        Guid guid_of(Handle h) const
        {
            std::lock_guard lock(m_mutex);
            auto it = m_handle_to_guid.find(h);
            if (it != m_handle_to_guid.end()) return it->second;
            return Guid::invalid();
        }

        // void bind_guid(Handle h, Guid guid)
        // {
        //     std::lock_guard lock(m_mutex);
        //     m_guid_map[guid] = h;
        //     m_handle_to_guid[h] = guid;
        // }

        Handle find_by_guid(Guid guid) const
        {
            std::lock_guard lock(m_mutex);
            auto it = m_guid_map.find(guid);
            return (it != m_guid_map.end()) ? it->second : Handle{};
        }

    private:
        void remove_unlocked(Handle h)
        {
            if (!m_versions.validate(h)) return;

            m_pool.destroy<T>(h);
            m_versions.remove(h);
            m_ref_counts[h.ofs / sizeof(T)] = 0;
        }

        void ensure_metadata(Handle h)
        {
            size_t i = h.ofs / sizeof(T);
            if (i >= m_ref_counts.size())
            {
                m_ref_counts.resize(i + 1, 0);
                m_versions.resize((i + 1) * sizeof(T));
            }
        }

        PoolAllocatorFH m_pool;
        VersionMap<T> m_versions;
        std::vector<uint32_t> m_ref_counts;

        std::unordered_map<Guid, Handle> m_guid_map;
        std::unordered_map<Handle, Guid> m_handle_to_guid;

        mutable std::mutex m_mutex;
    };


    class ResourceRegistry
    {
    public:
        template<typename T, typename... Args>
        Handle<T> add(const Guid& guid, Args&&... args)
        {
            return get_or_create_pool<T>()->add(guid, std::forward<Args>(args)...);
        }

        template<typename T>
        T& get(Handle<T> h)
        {
            return get_pool<T>()->get(h);
        }

        template<typename T>
        void remove(Handle<T> h)
        {
            get_pool<T>()->remove(h);
        }

        template<typename T>
        void retain(Handle<T> h)
        {
            get_pool<T>()->retain(h);
        }

        template<typename T>
        void release(Handle<T> h)
        {
            get_pool<T>()->release(h);
        }

        template<typename T>
        uint32_t use_count(Handle<T> h) const
        {
            return get_pool<T>()->use_count(h);
        }

        template<typename T>
        bool valid(Handle<T> h) const
        {
            return get_pool<T>()->valid(h);
        }

        template<typename T, typename F>
        void for_all(F&& f)
        {
            get_pool<T>()->for_each(std::forward<F>(f));
        }

        template<typename T>
        Handle<T> find_by_guid(Guid guid) const
        {
            return get_pool<T>()->find_by_guid(guid);
        }

        // template<typename T>
        // void bind_guid(Handle<T> h, Guid g)
        // {
        //     get_pool<T>()->bind_guid(h, g);
        // }

    private:
        std::unordered_map<std::type_index, std::unique_ptr<IResourcePool>> pools;

        template<typename T>
        ResourcePool<T>* get_pool()
        {
            auto it = pools.find(std::type_index(typeid(T)));
            if (it == pools.end())
                throw std::runtime_error("Resource type not registered");
            return static_cast<ResourcePool<T>*>(it->second.get());
        }

        template<typename T>
        const ResourcePool<T>* get_pool() const
        {
            auto it = pools.find(std::type_index(typeid(T)));
            if (it == pools.end())
                throw std::runtime_error("Resource type not registered");
            return static_cast<const ResourcePool<T>*>(it->second.get());
        }

        template<typename T>
        ResourcePool<T>* get_or_create_pool()
        {
            auto type = std::type_index(typeid(T));
            auto it = pools.find(type);
            if (it == pools.end())
            {
                auto pool = std::make_unique<ResourcePool<T>>();
                auto* raw_ptr = pool.get();
                pools[type] = std::move(pool);
                return raw_ptr;
            }
            return static_cast<ResourcePool<T>*>(it->second.get());
}
    };
#endif

} // namespace eeng

// namespace entt {
//   template<>
//   struct is_meta_pointer_like<eeng::Storage*> : std::true_type {};
// }
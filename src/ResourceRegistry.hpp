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
#include "PoolAllocatorFH.h"
// #include "Texture.hpp"

namespace eeng
{

} // namespace eeng

namespace eeng
{
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
        Storage& operator=(Storage&& other) noexcept {
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

    private:
        class IPool
        {
        public:
            virtual ~IPool() = default;
        };

        template<typename T>
        class Pool : public IPool
        {

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
    template<class T>
    class VersionMap
    {
        std::vector<handle_version_type> versions;

        const size_t elem_size = sizeof(T);

    public:
        VersionMap() : versions(0, version_null) {}

        void resize(size_t bytes)
        {
            versions.resize(bytes / elem_size);
        }

        void versionify(Handle<T>& h)
        {
            size_t index = h.ofs / elem_size;
            assert(index < versions.size());

            if (versions[index] == version_null)
                h.version = versions[index] = 1;
            else
                h.version = versions[index]; // = versions[index] + 1;
        }

        bool validate(const Handle<T>& h) const
        {
            if (h.version == version_null) return false;

            size_t index = h.ofs / elem_size;
            assert(index < versions.size());

            //        if (!h.version) std::cout << h << " has no version. ";
            //        if (!versions[index]) std::cout << h << ", index " << index << " is unversioned. ";
            //        if (h.version != versions[index]) std::cout << h << " version mismatch: " << h.version << " != " << versions[index];
            //        if (h.version && h.version == versions[index]) std::cout << h << " index " << index << " OK";
            //        std::cout << std::endl;

            return h.version == versions[index];
        }

        void remove(const Handle<T>& h)
        {
            size_t index = h.ofs / elem_size;
            assert(index < versions.size());

            //        versions[index] = version_null;
            versions[index]++;
        }

        void print() const
        {
            for (auto& v : versions)
                std::cout << v << ", ";
            std::cout << std::endl;
        }
    };

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
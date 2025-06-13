// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef Handle_h
#define Handle_h
#include <cstddef>
#include <cstdint>
#include "hash_combine.h"


namespace eeng
{
#if 0
    // -> Handle
    struct MetaHandle
    {
        uint32_t ofs = 0;
        uint32_t version = 0;
        entt::meta_type type = {};

        // template<typename T>
        // MetaHandle(Handle<T> h)
        //     : ofs(h.ofs), version(h.version), type(entt::resolve<T>()) {
        // }
        // MetaHandle() = default;

        bool valid() const { return version != 0 && type; }

        bool operator==(const MetaHandle& other) const {
            return ofs == other.ofs && version == other.version && type == other.type;
        }
    };
#endif

    // -- 

    using handle_ofs_type = size_t;
    using handle_version_type = uint16_t;

    const size_t handle_ofs_null = -1;

    // -> THandle
    template<class T>
    struct Handle
    {
        using value_type = T;
        using ptr_type = T*;

        handle_ofs_type ofs = handle_ofs_null; // -> ofs
        handle_version_type version = 0;

        void reset()
        {
            ofs = handle_ofs_null;
            version = 0;
        }

        bool operator== (Handle<T> rhs) const
        {
            if (ofs != rhs.ofs) return false;
            if (version != rhs.version) return false;
            return true;
        }

        // template<typename T>
        // Handle<T> cast() const
        // {
        //     if (type != entt::resolve<T>()) {
        //         throw std::bad_cast(); // or assert, or return invalid handle
        //     }
        //     return Handle<T>{ ofs, version };
        // }

        operator bool() const
        {
            return ofs != handle_ofs_null;
        }
    };
}

namespace std {
    template<typename T>
    struct hash<eeng::Handle<T>>
    {
        size_t operator()(const eeng::Handle<T>& h) const noexcept
        {
            return hash_combine(h.ofs, h.version);
        }
    };
}

#endif /* Handle_h */

// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef Handle_h
#define Handle_h
#include <cstddef>
#include <cstdint>
#include "hash_combine.h"

namespace eeng
{
    using handle_ofs_type = size_t;
    using handle_version_type = uint16_t;

    const size_t handle_ofs_null = -1;

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

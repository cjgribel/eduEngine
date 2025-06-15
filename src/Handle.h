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
    using handle_ver_type = uint16_t;
    constexpr handle_ofs_type  handle_ofs_null = std::numeric_limits<handle_ofs_type>::max();
    constexpr handle_ver_type  handle_ver_null = std::numeric_limits<handle_ver_type>::max();


    template<class T>
    struct Handle
    {
        using value_type = T;
        using ptr_type = T*;

        handle_ofs_type ofs;
        handle_ver_type ver;

        Handle() : ofs(handle_ofs_null), ver(handle_ver_null) {}
        Handle(handle_ofs_type ofs) : ofs(ofs), ver(handle_ver_null) {}
        Handle(handle_ofs_type ofs, handle_ver_type ver) : ofs(ofs), ver(ver) {}

        void reset()
        {
            ofs = handle_ofs_null;
            ver = handle_ver_null;
        }

        bool operator== (const Handle<T> rhs) const
        {
            if (ofs != rhs.ofs) return false;
            if (ver != rhs.ver) return false;
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
            return hash_combine(h.ofs, h.ver);
        }
    };
}

#endif /* Handle_h */

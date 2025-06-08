// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef MetaInfo_h
#define MetaInfo_h

#include <iostream>

namespace eeng
{
    struct TypeMetaInfo
    {
        std::string display_name;
        std::string tooltip;
    };

    struct DataMetaInfo
    {
        std::string display_name;
        std::string tooltip;
    };

    struct FuncMetaInfo
    {
        std::string display_name;
        std::string tooltip;
    };

    enum class MetaFlags : std::uint16_t
    {
        none = 0,
        read_only = 1 << 0,
        hidden = 1 << 1,
    };

    constexpr MetaFlags operator|(MetaFlags lhs, MetaFlags rhs)
    {
        return static_cast<MetaFlags>(
            static_cast<std::uint16_t>(lhs) |
            static_cast<std::uint16_t>(rhs)
            );
    }

    constexpr MetaFlags operator&(MetaFlags lhs, MetaFlags rhs)
    {
        return static_cast<MetaFlags>(
            static_cast<std::uint16_t>(lhs) &
            static_cast<std::uint16_t>(rhs)
            );
    }

    constexpr bool any(MetaFlags flags)
    {
        return static_cast<std::uint16_t>(flags) != 0;
    }
} // namespace eeng

#endif // MetaInfo_h
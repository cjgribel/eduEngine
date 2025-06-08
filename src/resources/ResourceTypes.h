// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef ResourceTypes_h
#define ResourceTypes_h
#include <cstddef>
#include <cstdint>
#include "Handle.h"
// #include "hash_combine.h"

namespace eeng
{
    struct MockResource1 
    {
        int x{};
        float y{};
    };

    struct MockResource2
    {
        size_t y = 0;
        Handle<MockResource1> ref1; // Reference to another resource
    };

} // namespace eeng

#endif // ResourceTypes_h

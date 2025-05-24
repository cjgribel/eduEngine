//
//  memaux.h
//  oengine
//
//  Created by Carl Johan Gribel on 2023-09-20.
//  Copyright © 2023 Carl Johan Gribel. All rights reserved.
//

#ifndef memaux_h
#define memaux_h

#define PoolMinAlignment alignof(std::max_align_t)

inline void aligned_alloc(void** ptr,
                          size_t size,
                          size_t alignment)
{
    assert(!*ptr);
#ifdef _WIN32
    *ptr = _aligned_malloc(size,
                           alignment);
    if (!*ptr) throw std::bad_alloc();
#else
    int allocres = posix_memalign(ptr, alignment, size);
    
    if (allocres)
    {
        if (allocres == EINVAL) std::cout << "The alignment argument was not a power of two, or was not a multiple of sizeof(void *)" << std::endl;
        if (allocres == ENOMEM) std::cout << "There was insufficient memory to fulfill the allocation request" << std::endl;
        throw std::bad_alloc();
    }
#endif
}

inline void aligned_free(void** ptr)
{
    assert(*ptr);
#ifdef _WIN32
    _aligned_free(*ptr);
#else
    free(*ptr);
#endif
    *ptr = nullptr;
}

// Round number down/up to closest multiple of alignment (power of 2)
// Game Engine Architecture 3rd Ed. page 430.
// https://stackoverflow.com/a/54962932/4484477
//
inline bool is_power_of_two(size_t x)
{
    return (x & (x - 1ul)) == 0;
}

inline size_t align_down(size_t x,
                         size_t alignment)
{
    assert(is_power_of_two(alignment));
//    assert((alignment & (alignment - 1ul)) == 0); // alignment must be power-of-2
    return ( (x - 1ul) & ~(alignment - 1ul) );
}

inline size_t align_up(size_t x,
                       size_t alignment)
{
    return align_down(x, alignment) + alignment;
}

// Custom untested implementation of std::align
// The aligning offset ('ofs') might be useful to extract.
// Todo: Compare with
// 1) Game Engine Architecture p 246
// 2) https://stackoverflow.com/a/227900
//
inline void* align(size_t alignment,
                   size_t size,
                   void*& ptr,
                   size_t& space)
{
    // Next multiple of alignment
    size_t round_up = align_up((size_t)ptr, alignment);
    // Increment needed in order to align ptr
    size_t align_ofs = round_up - (size_t)ptr;

    // Check if there is room for the requested allocation
    if (space < (size + align_ofs))
        return nullptr;

    // Align ptr and decrement space by offset
    ptr = (void*)round_up;
    space -= align_ofs;
    return ptr;
}

inline size_t align_ofs(void*& ptr,
                        size_t alignment)
{
    return align_up((size_t)ptr, alignment) - (size_t)ptr;
}


// Compute the next highest power-of-2 of 32-bit unsigned int
// Source: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
//inline unsigned next_power_of_two(unsigned n)
//{
//    n--;
//    n |= n >> 1;
//    n |= n >> 2;
//    n |= n >> 4;
//    n |= n >> 8;
//    n |= n >> 16;
//    n++;
//    return n;
//}

inline size_t next_power_of_two(size_t n)
{
    size_t result = 1;
    while (result < n) result <<= 1;
    
    return result;
}

#endif /* memaux_h */

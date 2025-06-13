// Created by Carl Johan Gribel 2022-2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef PoolAllocatorTFH_h
#define PoolAllocatorTFH_h

#include <typeindex> // std::type_index
#include <cassert>
#include <iostream> // std::cout
#include <memory> // std::unique_ptr
#include <utility> // std::exchange
#include <limits> // std::numeric_limits
#include <algorithm> // std::copy
#include <cstring> // std::memcpy
#include <stdexcept> // std::bad_alloc
#include <mutex> // std::mutex
#include "memaux.h"
#include "Handle.h"

namespace eeng {

    // --- PoolAllocator (Templated, Freelist, Handle) 2025 --------------------

    // * Raw memory allocator with alignment
    // * Compile-time type-safe
    // * Embedded singly-linked free-list
    // * Can expand & reallocate
    // * Can reset but not shrink

    // template<class T, size_t Alignment = alignof(std::variant<size_t, T>)>
    template<
        class T,
        class TIndex = std::size_t,
        size_t Alignment = std::max(alignof(T), alignof(TIndex))
    >
        requires (sizeof(T) >= sizeof(TIndex) && Alignment >= PoolMinAlignment && (Alignment % PoolMinAlignment) == 0)
    class PoolAllocatorTFH
    {
        using value_type = T;
        using cvalue_type = const T;
        using ptr_type = T*;
        using cptr_type = const T*;
        using index_type = TIndex;

        const size_t element_size = std::max(sizeof(T), sizeof(index_type));
        //    const size_t type_align = alignof(T);
        const index_type index_null = -1; //~0;

        void* m_pool = nullptr;   // Pool base address
        //    void*   m_ptr = nullptr;    // Current free address - TODO: std::atomic<void*>
        size_t  m_capacity = 0;     // Capacity in bytes
        //    size_t  m_remsize = 0;      // Remaining bytes - TODO: std::atomic<void*>

        // size_t  m_pool_alignment;
        // bool    m_can_expand; // TODO skip

        mutable std::recursive_mutex m_mutex;

        index_type free_first = index_null;
        index_type free_last = index_null;

        inline void assert_index(index_type index) const
        {
            assert(index != index_null);
            assert(index < m_capacity);
            assert(index % sizeof(T) == 0);
        }

        template<class Type>
        inline Type* ptr_at(void* ptr, index_type index)
        {
            assert_index(index);
            return reinterpret_cast<Type*>(static_cast<char*>(ptr) + index);
        }

        template<class Type>
        inline const Type* ptr_at(void* ptr, index_type index) const
        {
            assert_index(index);
            return reinterpret_cast<const Type*>(static_cast<char*>(ptr) + index);
        }

    public:
        PoolAllocatorTFH(size_t count = 0) 
            // : m_pool_alignment(Alignment)
        {
            //m_pool_alignment = align_up(pool_alignment, PoolMinAlignment);
            // m_can_expand = can_expand;

            resize(count * element_size);

            //#ifdef UNIT_TESTS
            std::cout << "Created pool(size = " << count
                << ", pool_alignment = " << Alignment << "): " << std::endl
                << "\tm_capacity " << m_capacity << std::endl
                << "\telement size " << element_size << std::endl;
                // << "\tm_pool_alignment " << Alignment << std::endl;
            //#endif
        }

        ~PoolAllocatorTFH()
        {
            if (m_pool)
            {
#ifdef _WIN32
                _aligned_free(m_pool);
#else
                free(m_pool);
#endif
                m_pool = nullptr;
            }
        }

        size_t capacity() const
        {
            return m_capacity;
        }

        // private
    //    template <class... Args>
    //    T* emplace_at(T* ptr, Args&&... args)
    //    {
    //        if constexpr(std::is_aggregate_v<T>)
    //            return ::new(ptr) T{std::forward<Args>(args)...};
    //        else
    //            return ::new(ptr) T(std::forward<Args>(args)...);
    //    }

        template<class... Args>
        Handle<value_type> create(Args&&... args)
        {
            std::lock_guard lock(m_mutex);
            // std::cout << __PRETTY_FUNCTION__ << std::endl;

            // Resize if free-list is empty
            if (free_first == index_null)
            {
                size_t new_capacity = next_power_of_two((unsigned)(m_capacity / element_size + 1)) * element_size;
                resize(new_capacity);
                //            std::cout << "Resized: m_capacity " << m_capacity << ", new_capacity " << new_capacity << " (element_size " << element_size << ")" << std::endl;
            }

            size_t index = free_first;
            void* ptr = (void*)((char*)m_pool + index);

            //        std::cout << "index " << index << ", ptr " << (unsigned long long)ptr << std::endl;

                    // Unlink first_free
            if (free_first == free_last)
                // No free elements left
                free_first = free_last = index_null;
            else
                free_first = get_free_diff(m_pool, free_first);

            if constexpr (std::is_aggregate_v<value_type>)
                ::new(ptr) value_type{ std::forward<Args>(args)... };
            else
                ::new(ptr) value_type(std::forward<Args>(args)...);

            std::cout << "created Handle " << index << std::endl;
            //        print_free();

            return Handle<value_type> { index };
        }

        void destroy(Handle<value_type> hnd)
        {
            assert(hnd);
            std::lock_guard lock(m_mutex);

            ptr_type elem_ptr = (ptr_type)((char*)m_pool + hnd.ofs);
            elem_ptr->~value_type();
            //::operator delete(elem_ptr);

    //        freelist.link_at_head(m_pool, hnd.index);
            if (free_first == index_null)
            {
                // Free-list is empty
                set_free_diff(m_pool, hnd.ofs, index_null);
                free_first = free_last = hnd.ofs;
            }
            else
            {
                set_free_diff(m_pool, hnd.ofs, free_first);
                free_first = hnd.ofs;
            }

            //        std::cout << "destroyed at " << hnd.ofs << std::endl;
        }

        value_type& get(Handle<value_type> handle)
        {
            std::lock_guard lock(m_mutex);

            return *ptr_at<value_type>(m_pool, handle.ofs);
            //return *static_cast<ptr_type>((void*)((char*)m_pool + Handle.ofs));
        }

        cvalue_type& get(Handle<value_type> handle) const
        {
            std::lock_guard lock(m_mutex);

            return *ptr_at<value_type>(m_pool, handle.ofs);
            // return *static_cast<ptr_type>((void*)((char*)m_pool + Handle.ofs));
        }

        index_type count_free() const
        {
            std::lock_guard lock(m_mutex);

            index_type index_count = 0;
            freelist_visitor([&](index_type i) {
                index_count++;
                });
            return index_count;
        }

        void dump_pool() const
        {
            // Print all elements.
            // [x] = used element
            // [i] = free element pointing to next free element at index i
            std::lock_guard lock(m_mutex);

            std::cout << "Pool " << "(" << capacity() / sizeof(T) << "): ";
            for (index_type index = 0; index < m_capacity; index += sizeof(T))
            {
                bool has_index = false;
                freelist_visitor([&](index_type i) {
                    has_index |= (i == index);
                    });

                index_type index_next = get_free_diff(m_pool, index);
                std::string index_str = (index_next == index_null ? "null" : std::to_string(index_next / sizeof(T)));
                if (has_index)      std::cout << "[" << index_str << "]"; // get_index_at(index) << "]";
                else                std::cout << "[x]";
            }
            std::cout << ". nbr free " << count_free();
            std::cout << ". freelist head = " << free_first;
            std::cout << std::endl;
        }

        /// @brief Visit all used elements in the pool.
        /// @tparam F The function type.
        /// @param f The function to call for each used element.
        /// @note O(2N). Uses dynamic allocation. Mainly intended for debug use.
        template<class F>
        void used_visitor(F&& f)
        {
            std::lock_guard lock(m_mutex);

            std::vector<bool> used(m_capacity / sizeof(T), true);
            freelist_visitor([&](index_type i)
                {
                    used[i / sizeof(T)] = false;
                });

            for (index_type index = 0;
                index < m_capacity;
                index += sizeof(T))
            {
                if (used[index / sizeof(T)])
                    f(*ptr_at<T>(m_pool, index));
            }
        }

    private:

        inline size_t get_free_diff(void* ptr, size_t diff_elem) const
        {
            //        diff_type* elem = reinterpret_cast<diff_type*>((char*)ptr + diff_elem);
            index_type* elem = static_cast<index_type*>((void*)((char*)ptr + diff_elem));
            //        diff_type* elem = (diff_type*)((char*)ptr + diff_elem);
            return *elem;
        }

        inline void set_free_diff(void* ptr, size_t diff_elem, size_t diff_value)
        {
            //        diff_type* elem = reinterpret_cast<diff_type*>((char*)ptr + diff_elem);
            index_type* elem = static_cast<index_type*>((void*)((char*)ptr + diff_elem));
            //        diff_type* elem = (diff_type*)((char*)ptr + diff_elem);
            *elem = diff_value;
        }

        // Link-in new elements at the back
        void expand_freelist(size_t old_capacity,
            size_t new_capacity)
        {
            // Assume everything is linked up until old_capacity-1
            // Link-in from old_capacity to old_capacity-1

            assert(new_capacity > old_capacity);
            assert(new_capacity >= element_size);
            if (new_capacity == old_capacity) return;

            // No free elements in old allocation
            if (free_first == index_null) free_first = old_capacity;

            // Link new elements at the back
            for (size_t b = old_capacity;
                b < new_capacity;
                b += element_size)
            {
                if (free_last != index_null)
                    set_free_diff(m_pool, free_last, b);
                free_last = b;

                //            std::ptrdiff_t* elem = (std::ptrdiff_t*)(m_pool + b);
                //            *elem = free_last;
                //            free_last = b;
                //            ptr_type elem = (ptr_type)(m_pool + b);
                //            *elem = free_last;
                //            free_last = b;
            }
            // The last free element links to null
            set_free_diff(m_pool, free_last, index_null);
            //        if (!old_capacity) free_first = 0;
                    // Put -1 in the last free spot
            //        void* free_last_ptr = m_pool + free_last;
            //        std::ptrdiff_t* elem = (std::ptrdiff_t*)(m_pool + free_last);
            //        *elem = -1;
        }

        // Creation, reallocation, clearing
        void resize(size_t size)
        {
            const size_t elem_size = sizeof(T);

            if (size == m_capacity) return;
            void* new_pool = nullptr;
            size_t new_capacity = size;

            std::cout << "Before resize: " << std::endl; dump_pool();

            if (new_capacity)
            {
#ifdef _WIN32
                pool = _aligned_malloc(capacity,
                    Alignment);
                if (!pool) throw std::bad_alloc();
#else
                int allocres = posix_memalign(&new_pool,
                    Alignment,
                    new_capacity);
                if (allocres)
                {
                    // The alignment argument was not a power of two, or was not a
                    // multiple of sizeof(void *).
                    if (allocres == EINVAL) std::cout << "EINVAL" << std::endl;
                    // There was insufficient memory to fulfill the allocation request.
                    if (allocres == ENOMEM) std::cout << "ENOMEM" << std::endl;
                    throw std::bad_alloc();
                }
#endif
            }

            // Copy old data to new location
            // Does not assume that the free-list is empty
#if 0
            if (m_pool && new_pool)
                memcpy(new_pool, m_pool, m_capacity);
#else
            if (m_pool && new_pool)
            {
                std::vector<bool> used(m_capacity / elem_size, true);
                freelist_visitor([&](index_type i) {
                    used[i / elem_size] = false;
                    });

                for (index_type index = 0; index < m_capacity; index += elem_size)
                {
                    if (used[index / elem_size])
                    {
                        T* src_elem_ptr = static_cast<T*>((void*)((char*)m_pool + index));
                        void* dest_elem_ptr = (char*)new_pool + index;

                        ::new(dest_elem_ptr) T(std::move(*src_elem_ptr));
                        src_elem_ptr->~T();
                    }
                    else
                        set_free_diff(new_pool, index, get_free_diff(m_pool, index));
                }
            }
#endif


            // Free old data
            if (m_pool)
            {
#ifdef _WIN32
                _aligned_free(m_pool);
#else
                free(m_pool);
#endif
            }

            //        std::cout << "Resize " << m_capacity << " -> " << new_capacity << std::endl;

            m_pool = new_pool;
            expand_freelist(m_capacity, new_capacity);
            m_capacity = new_capacity;

            std::cout << "After resize: " << std::endl; dump_pool();
        }

        template<class F>
        void freelist_visitor(F&& f) const
        {
            index_type cur_index = free_first;
            while (cur_index != index_null)
            {
                f(cur_index);
                cur_index = get_free_diff(m_pool, cur_index);
            }
        }
    };

} // namespace eeng

#endif /* Pool_h */

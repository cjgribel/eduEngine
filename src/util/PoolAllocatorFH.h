// Created by Carl Johan Gribel 2022-2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef PoolAllocatorFH_h
#define PoolAllocatorFH_h

#include <typeindex> // std::type_index
#include <cassert>
#include <iostream> // std::cout
#include <memory> // std::unique_ptr
#include <utility> // std::exchange
#include <limits> // std::numeric_limits
#include <algorithm> // std::copy
#include <cstring> // std::memcpy
#include <sstream> // std::ostringstream
#include <stdexcept> // std::bad_alloc
#include <mutex> // std::mutex
#include "memaux.h"
#include "Handle.h"

namespace eeng {

    // --- PoolAllocator (Freelist, Handle) 2025 -------------------------------

    // Runtime Type-Safe, Not Statically Type-Safe

    // * Raw memory allocator with alignment
    // * Runtime type-safe
    //   No type template - type is provided separately to create / destroy / get
    //      It is the responsibility of the user to always use the same type
    // * Embedded singly-linked free-list
    // * Can expand & reallocate
    // * Can reset but not shrink

    struct TypeInfo
    {
        std::type_index index;
        size_t          size;

        template<class T>
        static TypeInfo create()
        {
            return TypeInfo{ typeid(T), sizeof(T) };
        }
    };

    class PoolAllocatorFH
    {
        using index_type = std::size_t;

        const TypeInfo      m_type_info;
        const size_t        m_pool_alignment = 0ul;
        const index_type    m_index_null = -1;

        void* m_pool = nullptr;
        size_t      m_capacity = 0;     // Capacity in bytes
        index_type  m_free_first = m_index_null;
        index_type  m_free_last = m_index_null;

        mutable std::recursive_mutex m_mutex;

        inline void assert_index(index_type index) const
        {
            assert(index != m_index_null);
            assert(index < m_capacity);
            assert(index % m_type_info.size == 0);
        }

        template<class T>
        inline T* ptr_at(void* ptr, index_type index)
        {
            assert_index(index);
            return reinterpret_cast<T*>(static_cast<char*>(ptr) + index);
        }

        template<class T>
        inline const T* ptr_at(void* ptr, index_type index) const
        {
            assert_index(index);
            return reinterpret_cast<const T*>(static_cast<char*>(ptr) + index);
        }

        template<class T>
        inline T& val_at(void* ptr, index_type index)
        {
            return *ptr_at<T>(ptr, index);
            //        const_cast<const FreelistPool2*>(this)->get_at(ptr, index);
            //        return *reinterpret_cast<T*>((char*)ptr + index);
        }

        template<class T>
        inline const T& val_at(void* ptr, index_type index) const
        {
            return *ptr_at<T>(ptr, index);
            //        return *reinterpret_cast<T*>((char*)ptr + index);
        }

    public:

        //    FreelistPool2() = default;

        PoolAllocatorFH(
            const TypeInfo type_info,
            size_t pool_alignment = PoolMinAlignment) :
            m_type_info(type_info),
            m_pool_alignment(align_up(pool_alignment, PoolMinAlignment))
        {
            assert(type_info.size >= sizeof(index_type));
            //        assert(elem_size >= sizeof(index_type));

#if 0
            std::cout << "Created pool(size = [auto size 0]" //<< size
                << ", pool_alignment = " << pool_alignment << "): " << std::endl
                << "\tm_capacity " << m_capacity << std::endl
                << "\telement size " << m_elem_size << std::endl
                << "\tm_pool_alignment " << m_pool_alignment << std::endl;
#endif
        }

        //    FreelistPool2(FreelistPool2&& rhs) noexcept
        //    {
        //        m_elem_size         = std::exchange(rhs.m_elem_size, 0ul);
        //        m_pool_alignment    = std::exchange(rhs.m_pool_alignment, 0ul);
        //        m_pool              = std::exchange(rhs.m_pool, nullptr);
        //        m_capacity          = std::exchange(rhs.m_capacity, 0ul);
        //        m_free_first        = std::exchange(rhs.m_free_first, m_index_null);
        //        m_free_last         = std::exchange(rhs.m_free_last, m_index_null);
        //    }
        //
        //    FreelistPool2& operator=(FreelistPool2&& rhs) noexcept
        //    {
        //        if (this == &rhs) return *this;
        //
        //        m_elem_size         = std::exchange(rhs.m_elem_size, 0ul);
        //        m_pool_alignment    = std::exchange(rhs.m_pool_alignment, 0ul);
        //        m_pool              = std::exchange(rhs.m_pool, nullptr);
        //        m_capacity          = std::exchange(rhs.m_capacity, 0ul);
        //        m_free_first        = std::exchange(rhs.m_free_first, m_index_null);
        //        m_free_last         = std::exchange(rhs.m_free_last, m_index_null);
        //        return *this;
        //    }
        //
        //    FreelistPool2(const FreelistPool2&) = delete;
        //    FreelistPool2& operator=(const FreelistPool2&) = delete;

        ~PoolAllocatorFH()
        {
            if (m_pool)
                aligned_free(&m_pool);
        }

    public:
        size_t capacity() const
        {
            return m_capacity;
        }

        template<class T, class... Args>
        Handle<T> create(Args&&... args)
        {
            std::lock_guard lock(m_mutex);
            assert(m_type_info.index == std::type_index(typeid(T)));

            // Expand if free-list is empty
            if (m_free_first == m_index_null)
                expand<T>();

            // Next free element
            size_t index = m_free_first;
            void* ptr = ptr_at<void>(m_pool, index);

            // Unlink next free element from free-list
            if (m_free_first == m_free_last)
                m_free_first = m_free_last = m_index_null;
            else
                m_free_first = val_at<index_type>(m_pool, m_free_first);

            // Construct
            if constexpr (std::is_aggregate_v<T>)
                ::new(ptr) T{ std::forward<Args>(args)... };
            else
                ::new(ptr) T(std::forward<Args>(args)...);

            //        std::cout << "created handle " << index << std::endl;
            return Handle<T> {index};
        }

        template<class T>
        void destroy(Handle<T> handle)
        {
            std::lock_guard lock(m_mutex);
            assert(handle);
            assert(m_type_info.index == std::type_index(typeid(T)));

            T* elem_ptr = ptr_at<T>(m_pool, handle.ofs);
            elem_ptr->~T();

            // Link element to free-list
            if (m_free_first == m_index_null)
            {
                // Free-list is empty
                val_at<index_type>(m_pool, handle.ofs) = m_index_null;
                m_free_first = m_free_last = handle.ofs;
            }
            else
            {
                val_at<index_type>(m_pool, handle.ofs) = m_free_first;
                m_free_first = handle.ofs;
            }
        }

        template<class T>
        T& get(Handle<T> handle)
        {
            std::lock_guard lock(m_mutex);
            assert(m_type_info.index == std::type_index(typeid(T)));

            return *ptr_at<T>(m_pool, handle.ofs);
        }

        template<class T>
        T& get(Handle<T> handle) const
        {
            std::lock_guard lock(m_mutex);

            assert(m_type_info.index == std::type_index(typeid(T)));

            return *ptr_at<T>(m_pool, handle.ofs);
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

        /// @brief Convert the pool state to a string representation.
        /// @return A string describing the current state of the pool.
        std::string to_string() const
        {
            std::lock_guard lock(m_mutex);
            std::ostringstream oss;

            // 1) Summary line
            oss << "PoolAllocatorFH: capacity=" << (m_capacity / m_type_info.size)
                << ", free=" << count_free()
                << ", head=" << m_free_first
                << "\n";

            // 2) Free-list chain
            oss << "  free-list: ";
            index_type cur = m_free_first;
            while (cur != m_index_null)
            {
                oss << cur / m_type_info.size << " -> ";
                cur = val_at<index_type>(m_pool, cur);
            }
            oss << "null\n";

            // 3) Layout (used vs free)
            oss << "  layout: ";
            for (index_type idx = 0; idx < m_capacity; idx += m_type_info.size)
            {
                bool isFree = false;
                freelist_visitor([&](index_type i) { if (i == idx) isFree = true; });
                if (isFree) oss << "[F]";
                else       oss << "[U]";
            }
            oss << "\n";

            return oss.str();
        }

        // Traverse used elements in order
        // O(2N). Uses dynamic allocation. Mainly intended for debug use.
        template<class T, class F>
        void used_visitor(F&& f)
        {
            std::lock_guard lock(m_mutex);

            std::vector<bool> used(m_capacity / m_type_info.size, true);
            freelist_visitor([&](index_type i) {
                used[i / m_type_info.size] = false;
                });

            for (index_type index = 0;
                index < m_capacity;
                index += m_type_info.size)
            {
                if (used[index / m_type_info.size])
                    f(val_at<T>(m_pool, index));
            }
        }

    private:

        template<class T>
        void expand()
        {
            //        std::cout << "Before expand: " << std::endl; dump_pool();

            size_t prev_capacity = m_capacity;
            size_t new_capacity = next_power_of_two(m_capacity / m_type_info.size + 1ull) * m_type_info.size;

            resize<T>(new_capacity);
            expand_freelist(prev_capacity, m_capacity);

            //        std::cout << "After resize: " << std::endl; dump_pool();
        }

        // Link-in new elements at the back
        void expand_freelist(
            size_t old_capacity,
            size_t new_capacity)
        {
            // Assume everything is linked up until old_capacity-1:
            // link-in from old_capacity to new_capacity-1

            // std::lock_guard lock(m_mutex);

            assert(new_capacity > old_capacity);
            assert(new_capacity >= m_type_info.size);
            if (new_capacity == old_capacity) return;

            // No free elements in old allocation
            if (m_free_first == m_index_null) m_free_first = old_capacity;

            // Link new elements at the back
            for (size_t i = old_capacity;
                i < new_capacity;
                i += m_type_info.size)
            {
                if (m_free_last != m_index_null)
                    val_at<index_type>(m_pool, m_free_last) = i;
                m_free_last = i;
            }
            // The last free element links to null
            val_at<index_type>(m_pool, m_free_last) = m_index_null;
        }

        // Resize to a new capacity larger than the current one.
        // All live (non-free) elements are moved to the new buffer.
        // The freelist is preserved but not expanded; new elements must be linked via expand_freelist().
        //
        // Shrinking the pool is not supported: reducing capacity may invalidate existing objects
        // or corrupt the freelist structure.
        template<class T>
        void resize(size_t size)
        {
            // std::lock_guard lock(m_mutex);
            assert(size >= m_capacity && "Shrinking the pool is not supported");
            if (size == m_capacity) return;

            void* prev_pool = m_pool; m_pool = nullptr;
            size_t prev_capacity = m_capacity;
            m_capacity = size;

            if (m_capacity)
                aligned_alloc(&m_pool, m_capacity, m_pool_alignment);

            // Copy data to new location
            // Does not assume that the free-list is empty
    //        if (m_pool && new_pool)
    //            memcpy(new_pool, m_pool, m_capacity);
            if (prev_pool && m_pool)
            {
                //            if constexpr (std::is_standard_layout_v<T>)
                //            {
                //                memcpy(m_pool, prev_pool, prev_capacity);
                //                std::cout << "HELLO FROM SLT" << std::endl;
                //            }
                //            else
                //            {
                //                std::cout << "HELLO FROM NON-SLT" << std::endl;
                std::vector<bool> used(prev_capacity / m_type_info.size, true);
                freelist_visitor([&](index_type i) {
                    used[i / m_type_info.size] = false;
                    });

                for (index_type index = 0; index < prev_capacity; index += m_type_info.size)
                {
                    if (used[index / m_type_info.size])
                    {
                        T* src_elem_ptr = ptr_at<T>(prev_pool, index);
                        void* dest_elem_ptr = ptr_at<void>(m_pool, index);

                        ::new(dest_elem_ptr) T(std::move(*src_elem_ptr));
                        src_elem_ptr->~T();
                    }
                    else
                        val_at<index_type>(m_pool, index) = val_at<index_type>(prev_pool, index);
                }
                //            }
            }

            if (prev_pool)
                aligned_free(&prev_pool);
        }

        // Traverse free elements in the order in which they were added
        // O(N)
        template<class F>
        void freelist_visitor(F&& f) const
        {
            index_type cur_index = m_free_first;
            while (cur_index != m_index_null)
            {
                f(cur_index);
                cur_index = val_at<index_type>(m_pool, cur_index); // get_free_diff(m_pool, cur_index);
            }
        }
    };
}
#endif /* Pool_h */

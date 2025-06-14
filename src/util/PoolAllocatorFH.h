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

    public:

        PoolAllocatorFH(
            const TypeInfo type_info,
            size_t pool_alignment = PoolMinAlignment) :
            m_type_info(type_info),
            m_pool_alignment(align_up(pool_alignment, PoolMinAlignment))
        {
            assert(type_info.size >= sizeof(index_type));
        }

        PoolAllocatorFH(const PoolAllocatorFH&) = delete;
        PoolAllocatorFH& operator=(const PoolAllocatorFH&) = delete;
        PoolAllocatorFH(PoolAllocatorFH&&) = delete;
        PoolAllocatorFH& operator=(PoolAllocatorFH&&) = delete;

        ~PoolAllocatorFH()
        {
#if 0
            // Require that no elements remain in use
            const auto total_slots = m_capacity / m_type_info.size;
            assert(count_free() == static_cast<index_type>(total_slots)
                && "FreelistPoolFH destroyed with live elements still allocated");
#endif

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
                m_free_first = *ptr_at<index_type>(m_pool, m_free_first);

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
                *ptr_at<index_type>(m_pool, handle.ofs) = m_index_null;
                m_free_first = m_free_last = handle.ofs;
            }
            else
            {
                *ptr_at<index_type>(m_pool, handle.ofs) = m_free_first;
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
                cur = *ptr_at<index_type>(m_pool, cur);
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
                    f(*ptr_at<T>(m_pool, index));
            }
        }

    private:

        template<class T>
        void expand()
        {
            size_t prev_capacity = m_capacity;
            size_t new_capacity = next_power_of_two(m_capacity / m_type_info.size + 1ull) * m_type_info.size;

            resize<T>(new_capacity);
            expand_freelist(prev_capacity, m_capacity);
        }

        /// @brief Expand the freelist to a new capacity.
        /// @param old_capacity The previous capacity in bytes.
        /// @param new_capacity The new capacity in bytes.
        void expand_freelist(
            size_t old_capacity,
            size_t new_capacity)
        {
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
                    *ptr_at<index_type>(m_pool, m_free_last) = i;
                m_free_last = i;
            }
            // The last free element links to null
            *ptr_at<index_type>(m_pool, m_free_last) = m_index_null;
        }

        /// @brief Resize to a new capacity larger than the current one.
        /// @tparam T The type of elements in the pool.
        /// @param size The new size in bytes.
        /// @details All elements are moved to the new buffer.
        //          The freelist is preserved but not expanded:
        //          new elements must be linked via expand_freelist().
        //          Shrinking the pool is not supported
        template<class T>
        void resize(size_t size)
        {
            assert(size >= m_capacity && "Shrinking the pool is not supported");
            if (size == m_capacity) return;

            void* prev_pool = m_pool; m_pool = nullptr;
            size_t prev_capacity = m_capacity;
            m_capacity = size;

            if (m_capacity)
                aligned_alloc(&m_pool, m_capacity, m_pool_alignment);

            // Copy data to new location
            if (prev_pool && m_pool)
            {
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
                        *ptr_at<index_type>(m_pool, index) = *ptr_at<index_type>(prev_pool, index);
                }
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
                cur_index = *ptr_at<index_type>(m_pool, cur_index);
            }
        }
    };
}
#endif /* Pool_h */

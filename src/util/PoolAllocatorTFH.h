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
        const index_type index_null = -1;

        void* m_pool = nullptr;   // Pool base address
        size_t m_capacity = 0;     // Capacity in bytes

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
        explicit PoolAllocatorTFH(size_t count = 0)
        {
            if (count)
            {
                resize(count * element_size);
                expand_freelist(0, m_capacity);
            }
        }

        // Disable copy
        PoolAllocatorTFH(const PoolAllocatorTFH&) = delete;
        PoolAllocatorTFH& operator=(const PoolAllocatorTFH&) = delete;

        // Move-ctor
        PoolAllocatorTFH(PoolAllocatorTFH&& rhs) noexcept
        {
            std::lock_guard lock(rhs.m_mutex);

            m_pool = std::exchange(rhs.m_pool, nullptr);
            m_capacity = std::exchange(rhs.m_capacity, 0);
            free_first = std::exchange(rhs.free_first, index_null);
            free_last = std::exchange(rhs.free_last, index_null);
        }

        // Move-assg
        PoolAllocatorTFH& operator=(PoolAllocatorTFH&& rhs) noexcept
        {
            std::scoped_lock lock(m_mutex, rhs.m_mutex);

            if (this != &rhs)
            {
                // Destroy used elements
                if constexpr (!std::is_trivially_destructible_v<T>)
                    used_visitor([&](T& elem) { elem.~T(); });

                // free our current storage
                if (m_pool) aligned_free(&m_pool);

                // steal theirs
                m_pool = std::exchange(rhs.m_pool, nullptr);
                m_capacity = std::exchange(rhs.m_capacity, 0);
                free_first = std::exchange(rhs.free_first, index_null);
                free_last = std::exchange(rhs.free_last, index_null);
            }
            return *this;
        }

        ~PoolAllocatorTFH()
        {
            // Destroy used elements
            if constexpr (!std::is_trivially_destructible_v<T>)
                used_visitor([&](T& elem) { elem.~T(); });

            if (m_pool)
                aligned_free(&m_pool);
        }

        size_t capacity() const
        {
            return m_capacity;
        }

        template<class... Args>
        Handle<value_type> create(Args&&... args)
        {
            std::lock_guard lock(m_mutex);

            // Resize if free-list is empty
            if (free_first == index_null) expand();

            size_t index = free_first;
            void* ptr = (void*)((char*)m_pool + index);

            // Unlink first_free
            if (free_first == free_last)
                // No free elements left
                free_first = free_last = index_null;
            else
                free_first = *ptr_at<index_type>(m_pool, free_first);

            if constexpr (std::is_aggregate_v<value_type>)
                ::new(ptr) value_type{ std::forward<Args>(args)... };
            else
                ::new(ptr) value_type(std::forward<Args>(args)...);

            return Handle<value_type> { index };
        }

        void destroy(Handle<value_type> handle)
        {
            assert(handle);
            std::lock_guard lock(m_mutex);

            if constexpr (!std::is_trivially_destructible_v<T>)
                ptr_at<value_type>(m_pool, handle.ofs)->~value_type();

            if (free_first == index_null)
            {
                // Free-list is empty
                *ptr_at<index_type>(m_pool, handle.ofs) = index_null;
                free_first = free_last = handle.ofs;
            }
            else
            {
                *ptr_at<index_type>(m_pool, handle.ofs) = free_first;
                free_first = handle.ofs;
            }
        }

        value_type& get(Handle<value_type> handle)
        {
            std::lock_guard lock(m_mutex);

            return *ptr_at<value_type>(m_pool, handle.ofs);
        }

        cvalue_type& get(Handle<value_type> handle) const
        {
            std::lock_guard lock(m_mutex);

            return *ptr_at<value_type>(m_pool, handle.ofs);
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
            oss << "PoolAllocatorTFH: capacity=" << (m_capacity / sizeof(T))
                << ", free=" << count_free()
                << ", head=" << free_first
                << "\n";

            // 2) Free-list chain
            oss << "  free-list: ";
            index_type cur = free_first;
            while (cur != index_null)
            {
                oss << cur / sizeof(T) << " -> ";
                cur = *ptr_at<index_type>(m_pool, cur);
            }
            oss << "null\n";

            // 3) Layout (used vs free)
            oss << "  layout: ";
            for (index_type idx = 0; idx < m_capacity; idx += sizeof(T))
            {
                bool isFree = false;
                freelist_visitor([&](index_type i) { if (i == idx) isFree = true; });
                if (isFree) oss << "[F]";
                else {
                    const auto& value = *ptr_at<T>(m_pool, idx);
                    if constexpr (requires { oss << value; })
                        oss << "[" << value << "]";
                    else
                        oss << "[U]";
                }
            }
            oss << "\n";

            return oss.str();
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

        void expand()
        {
            size_t prev_capacity = m_capacity;
            size_t new_capacity = next_power_of_two(m_capacity / sizeof(T) + 1ull) * sizeof(T);

            resize(new_capacity);
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
            assert(new_capacity >= sizeof(T));
            if (new_capacity == old_capacity) return;

            // No free elements in old allocation
            if (free_first == index_null) free_first = old_capacity;

            // Link new elements at the back
            for (size_t i = old_capacity;
                i < new_capacity;
                i += sizeof(T))
            {
                if (free_last != index_null)
                    *ptr_at<index_type>(m_pool, free_last) = i;
                free_last = i;
            }
            // The last free element links to null
            *ptr_at<index_type>(m_pool, free_last) = index_null;
        }

        /// @brief Resize to a new capacity larger than the current one.
        /// @tparam T The type of elements in the pool.
        /// @param size The new size in bytes.
        /// @details All elements are moved to the new buffer.
        //          The freelist is preserved but not expanded:
        //          new elements must be linked via expand_freelist().
        //          Shrinking the pool is not supported
        void resize(size_t size)
        {
            assert(size >= m_capacity && "Shrinking the pool is not supported");
            if (size == m_capacity) return;

            void* prev_pool = m_pool; m_pool = nullptr;
            size_t prev_capacity = m_capacity;
            m_capacity = size;

            if (m_capacity)
                aligned_alloc(&m_pool, m_capacity, Alignment);

            // Copy data to new location
            if (prev_pool && m_pool)
            {
                std::vector<bool> used(prev_capacity / sizeof(T), true);
                freelist_visitor([&](index_type i) {
                    used[i / sizeof(T)] = false;
                    });

                for (index_type index = 0; index < prev_capacity; index += sizeof(T))
                {
                    if (used[index / sizeof(T)])
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

        template<class F>
        void freelist_visitor(F&& f) const
        {
            index_type cur_index = free_first;
            while (cur_index != index_null)
            {
                f(cur_index);
                cur_index = *ptr_at<index_type>(m_pool, cur_index);
            }
        }
    };

} // namespace eeng

#endif /* Pool_h */

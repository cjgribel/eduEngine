//  Created by Carl Johan Gribel 2024-2025
//  Licensed under the MIT License. See LICENSE file for details.

#ifndef VecTree_h
#define VecTree_h

#include <iostream>
#include <vector>
#include <queue>
#include <stack>
#include <cassert>

#define VecTree_NullIndex -1

template<class T>
struct TreeNode
{
    unsigned m_nbr_children = 0;    // Nbr of children
    unsigned m_branch_stride = 1;   // Branch size including this node
    unsigned m_parent_ofs = 0;      // Distance to parent, relative parent. 0 = root.
    T m_payload;                    // Payload
};

/**
Sequential tree representation optimized for depth-first traversal.
Nodes are organized in pre-order, which means that the first child of a node is located directly after the node.
Each node has information about number children, branch stride and parent offset.
The tree can be traversed is different ways.
*/
template <class PayloadType>
    requires requires(PayloadType a, PayloadType b) { { a == b } -> std::convertible_to<bool>; }
class VecTree
{
    using TreeNodeType = TreeNode<PayloadType>;
    std::vector<TreeNodeType> nodes;

public:
    VecTree() = default;

    /// @brief Find index of a node O(N)
    /// @param payload Payload to search for
    /// @return Index Node index
    size_t find_node_index(const PayloadType& payload) const
    {
        auto it = std::find_if(nodes.begin(), nodes.end(),
            [&payload](const TreeNodeType& node)
            {
                return payload == node.m_payload;
            });
        if (it == nodes.end()) return VecTree_NullIndex;
        return std::distance(nodes.begin(), it);
    }

    inline size_t size() const
    {
        return nodes.size();
    }

    void clear()
    {
        nodes.clear();
    }

    void reserve(size_t size)
    {
        nodes.reserve(size);
    }

    bool contains(const PayloadType& payload) const
    {
        return find_node_index(payload) != VecTree_NullIndex;
    }

    const PayloadType& get_payload_at(size_t index) const
    {
        assert(index != VecTree_NullIndex);
        return nodes[index].m_payload;
    }

    PayloadType& get_payload_at(size_t index)
    {
        assert(index != VecTree_NullIndex);
        return nodes[index].m_payload;
    }

    // Used for debug pritning...
    auto get_node_info(const PayloadType& payload) const
    {
        auto index = find_node_index(payload);
        assert(index != VecTree_NullIndex);
        const auto& node = nodes[index];
        return std::make_tuple(node.m_nbr_children, node.m_branch_stride, node.m_parent_ofs);
    }

    // For inspection (and transform traversal?)
    auto get_node_info_at(size_t index) const
    {
        assert(index != VecTree_NullIndex);
        const auto& node = nodes[index];
        return std::make_tuple(node.m_payload, node.m_nbr_children, node.m_branch_stride, node.m_parent_ofs);
    }

    auto get_branch_size(const PayloadType& payload)
    {
        auto index = find_node_index(payload);
        assert(index != VecTree_NullIndex);
        return nodes[index].m_branch_stride;
    }

    auto get_nbr_children(const PayloadType& payload)
    {
        auto index = find_node_index(payload);
        assert(index != VecTree_NullIndex);
        return nodes[index].m_nbr_children;
    }

    auto get_parent_ofs(const PayloadType& payload)
    {
        auto index = find_node_index(payload);
        assert(index != VecTree_NullIndex);
        return nodes[index].m_parent_ofs;
    }

    bool is_root(const PayloadType& payload) const
    {
        auto [nbr_children, branch_stride, parent_ofs] = get_node_info(payload);
        return parent_ofs == 0;
    }

    bool is_leaf(const PayloadType& payload) const
    {
        auto [nbr_children, branch_stride, parent_ofs] = get_node_info(payload);
        return nbr_children == 0;
    }

    size_t get_parent_index(const PayloadType& payload) const
    {
        auto node_index = find_node_index(payload);
        assert(node_index != VecTree_NullIndex);
        // Error if root
        assert(nodes[node_index].m_parent_ofs);

        return node_index - nodes[node_index].m_parent_ofs;
    }

    PayloadType& get_parent(const PayloadType& payload) const
    {
        return nodes[get_parent_index(payload)].m_payload;
    }

    PayloadType& get_parent(const PayloadType& payload)
    {
        return nodes[get_parent_index(payload)].m_payload;
    }

    bool is_descendant_of(const PayloadType& payload1, const PayloadType& payload2)
    {
        bool is_child = false;
        ascend(payload1, [&](auto& payload, size_t index) {
            if (payload == payload1) return;
            if (payload == payload2) is_child = true;
            });
        return is_child;
    }

    bool is_last_sibling(size_t index) const
    {
        assert(index < nodes.size());
        const auto& node = nodes[index];
        const auto  stride = node.m_branch_stride;

        // 1) Top‐level (root): check if the next element is another root
        if (node.m_parent_ofs == 0) {
            size_t next = index + stride;
            // If out of bounds or the next node has parent_ofs==0, we're last root
            return next >= nodes.size() || nodes[next].m_parent_ofs == 0;
        }

        // 2) Child: compute the parent’s branch range
        size_t parent_index = index - node.m_parent_ofs;
        size_t parent_end = parent_index + nodes[parent_index].m_branch_stride;
        size_t next_sibling = index + stride;

        // If next_sibling is at or beyond the parent’s branch end, we're the last child
        return next_sibling >= parent_end;
    }

    /// @brief Find by payload then call the index‐based helper
    bool is_last_sibling(const PayloadType& payload) const
    {
        auto idx = find_node_index(payload);
        assert(idx != VecTree_NullIndex);
        return is_last_sibling(idx);
    }

    void reparent(const PayloadType& payload, const PayloadType& parent_payload)
    {
        assert(!is_descendant_of(parent_payload, payload));
        auto node_index = find_node_index(payload);
        auto node_branch_stride = get_branch_size(payload);

        // Move branch to a temporary buffer
        std::deque<TreeNodeType> branch;
        branch.resize(node_branch_stride);
        std::move(
            nodes.begin() + node_index,
            nodes.begin() + node_index + node_branch_stride,
            branch.begin()
        );

        // Remove branch from its current parent
        erase_branch_at_index(node_index);
        // erase_branch(payload);

        // Reinsert branch under new parent
        insert(branch.front().m_payload, parent_payload);
        for (size_t i = 1; i < branch.size(); i++)
        {
            auto& node = branch[i];
            auto& parent_node = branch[i - node.m_parent_ofs];
            insert(node.m_payload, parent_node.m_payload);
        }
    }

    void unparent(const PayloadType& payload)
    {
        auto node_index = find_node_index(payload);
        auto node_branch_stride = get_branch_size(payload);

        // Move branch to a temporary buffer
        std::deque<TreeNodeType> branch;
        branch.resize(node_branch_stride);
        std::move(
            nodes.begin() + node_index,
            nodes.begin() + node_index + node_branch_stride,
            branch.begin()
        );

        // Remove branch from its current parent
        erase_branch_at_index(node_index);
        // erase_branch(payload);

        // Reinsert branch under new parent
        insert_as_root(branch.front().m_payload);
        for (size_t i = 1; i < branch.size(); i++)
        {
            auto& node = branch[i];
            auto& parent_node = branch[i - node.m_parent_ofs];
            insert(node.m_payload, parent_node.m_payload);
        }
    }

    void insert_as_root(const PayloadType& payload)
    {
        nodes.insert(
            nodes.end(),
            TreeNodeType{
                .m_nbr_children = 0,
                .m_branch_stride = 1,
                .m_parent_ofs = 0, // root
                .m_payload = payload });
    }

    /// @brief Insert a node
    /// @param payload Payload to insert
    /// @param parent_payload Payload of parent node.
    /// @return True if insertion was successfull, false otherwise
    bool insert(
        PayloadType const& payload,
        PayloadType const& parent_payload
    ) {
        // Find the parent
        size_t parent_idx = find_node_index(parent_payload);
        if (parent_idx == VecTree_NullIndex)
            return false;

        // Update branch_stride of ancestors
        for (size_t idx = parent_idx; ; )
        {
            nodes[idx].m_branch_stride += 1;
            if (nodes[idx].m_parent_ofs == 0)
                break;
            idx -= nodes[idx].m_parent_ofs;
        }

        // Update parent_ofs of in-range trailing nodes
        for (size_t i = parent_idx + 1; i < nodes.size(); ++i)
        {
            // stop at a subsequent root
            if (nodes[i].m_parent_ofs == 0)
                break;
            auto dist = static_cast<unsigned>(i - parent_idx);
            if (nodes[i].m_parent_ofs >= dist)
                nodes[i].m_parent_ofs += 1;
        }

        // Increment parent child‐count
        nodes[parent_idx].m_nbr_children += 1;

        // Insert the new node
        nodes.insert(
            nodes.begin() + (parent_idx + 1),
            TreeNodeType{
                .m_nbr_children = 0,
                .m_branch_stride = 1,
                .m_parent_ofs = 1,
                .m_payload = payload });

        return true;
    }

private:
    // Core branch-erasure by index (no payload search)
    bool erase_branch_at_index(size_t node_index)
    {
        assert(node_index < nodes.size());
        auto const branch_stride = nodes[node_index].m_branch_stride;
        auto const parent_ofs = nodes[node_index].m_parent_ofs;

        // Special-case: root
        if (parent_ofs == 0) {
            nodes.erase(nodes.begin() + node_index,
                nodes.begin() + node_index + branch_stride);
            return true;
        }

        // Update branch_stride of ancestors
        size_t parent_idx = node_index - parent_ofs;
        for (size_t idx = parent_idx; ; idx -= nodes[idx].m_parent_ofs)
        {
            nodes[idx].m_branch_stride -= branch_stride;
            if (nodes[idx].m_parent_ofs == 0)
                break;
        }

        // Update parent_ofs of in-range trailing nodes
        for (size_t i = node_index + branch_stride; i < nodes.size(); ++i)
        {
            if (nodes[i].m_parent_ofs == 0) break;
            auto dist = i - parent_idx;
            if (nodes[i].m_parent_ofs >= dist)
                nodes[i].m_parent_ofs -= branch_stride;
        }

        // Decrement parent's child count and erase
        nodes[parent_idx].m_nbr_children--;
        nodes.erase(
            nodes.begin() + node_index,
            nodes.begin() + node_index + branch_stride);

        return true;
    }

public:
    /// @brief Erase a node and its entire branch by payload lookup
    bool erase_branch(const PayloadType& payload)
    {
        auto node_index = find_node_index(payload);
        if (node_index == VecTree_NullIndex)
            return false;
        return erase_branch_at_index(node_index);
    }

    // --- Depth-first without level information (fast) -----------------------

private:
    //–– by start‐index
    template<class T, class F>
    static void traverse_depthfirst_impl(
        T& self,
        size_t start_index,
        F&& func)
    {
        auto& nodes = self.nodes;
        if (nodes.empty())
            return;
        assert(start_index != VecTree_NullIndex && start_index < nodes.size());

        size_t stride = nodes[start_index].m_branch_stride;
        for (size_t offset = 0; offset < stride; ++offset)
        {
            size_t idx = start_index + offset;
            auto& node = nodes[idx];

            const size_t parent_idx =
                node.m_parent_ofs ?
                idx - node.m_parent_ofs
                : VecTree_NullIndex;
            auto parent_ptr =
                parent_idx != VecTree_NullIndex ?
                &nodes[parent_idx].m_payload
                : nullptr;

            func(&node.m_payload, parent_ptr, idx, parent_idx);
            // func(nodes[start_index + offset].m_payload, start_index + offset);
        }
    }

    //–– by start‐payload
    template<class T, class F>
    static void traverse_depthfirst_impl(
        T& self,
        const PayloadType& start_payload,
        F&& func)
    {
        traverse_depthfirst_impl(
            self,
            self.find_node_index(start_payload),
            std::forward<F>(func)
        );
    }

    //–– entire forest
    template<class T, class F>
    static void traverse_depthfirst_impl(
        T& self,
        F&& func)
    {
        size_t i = 0;
        while (i < self.size()) {
            traverse_depthfirst_impl(
                self,
                i,
                std::forward<F>(func)
            );
            i += self.nodes[i].m_branch_stride;
        }
    }

public:
    /// @brief Traverse in depth-first order (const overload)
    /// @tparam F Callable type, 
    ///         invocable as void(const PayloadType*, const PayloadType*, size_t, size_t).
    /// @param start_index Index of the node to start traversal from.
    /// @param func Callable with signature 
    ///        void(const PayloadType* node, 
    ///             const PayloadType* parent, 
    ///             size_t node_index, 
    ///             size_t parent_index).
    /// @note The tree is optimized for depth-first traversal.
    template<class F>
        requires std::invocable<F, const PayloadType*, const PayloadType*, size_t, size_t>
    void traverse_depthfirst(
        size_t start_index,
        F&& func) const
    {
        traverse_depthfirst_impl(
            *this,
            start_index,
            std::forward<F>(func));
    }

    /// @brief Traverse in depth-first order (non-const overload)
    /// @tparam F Callable type, 
    ///         invocable as void(PayloadType*, PayloadType*, size_t, size_t).
    /// @param start_index Index of the node to start traversal from.
    /// @param func Callable with signature 
    ///        void(PayloadType* node, 
    ///             PayloadType* parent, 
    ///             size_t node_index, 
    ///             size_t parent_index).
    /// @note The tree is optimized for depth-first traversal.
    template<class F>
        requires std::invocable<F, PayloadType*, PayloadType*, size_t, size_t>
    void traverse_depthfirst(
        size_t start_index,
        F&& func)
    {
        traverse_depthfirst_impl(
            *this,
            start_index,
            std::forward<F>(func));
    }

    /// @brief Traverse in depth-first order (const overload)
    /// @tparam F Callable type, 
    ///         invocable as void(const PayloadType*, const PayloadType*, size_t, size_t).
    /// @param start_payload Node to start traversal from.
    /// @param func Callable with signature 
    ///        void(const PayloadType* node, 
    ///             const PayloadType* parent, 
    ///             size_t node_index, 
    ///             size_t parent_index).
    /// @note The tree is optimized for depth-first traversal.
    template<class F>
        requires std::invocable<F, const PayloadType*, const PayloadType*, size_t, size_t>
    void traverse_depthfirst(
        const PayloadType& start_payload,
        F&& func) const
    {
        traverse_depthfirst_impl(
            *this,
            start_payload,
            std::forward<F>(func));
    }

    /// @brief Traverse in depth-first order (non-const overload)
    /// @tparam F Callable type, 
    ///         invocable as void(PayloadType*, PayloadType*, size_t, size_t).
    /// @param start_payload Node to start traversal from.
    /// @param func Callable with signature 
    ///        void(PayloadType* node, 
    ///             PayloadType* parent, 
    ///             size_t node_index, 
    ///             size_t parent_index).
    /// @note The tree is optimized for depth-first traversal.
    template<class F>
        requires std::invocable<F, PayloadType*, PayloadType*, size_t, size_t>
    void traverse_depthfirst(
        const PayloadType& start_payload,
        F&& func)
    {
        traverse_depthfirst_impl(
            *this,
            start_payload,
            std::forward<F>(func));
    }

    /// @brief Traverse in depth-first order (const overload)
    /// @tparam F Callable type, 
    ///         invocable as void(const PayloadType*, const PayloadType*, size_t, size_t).
    /// @param func Callable with signature 
    ///        void(const PayloadType* node, 
    ///             const PayloadType* parent, 
    ///             size_t node_index, 
    ///             size_t parent_index).
    /// @note The tree is optimized for depth-first traversal.
    template<class F>
        requires std::invocable<F, const PayloadType*, const PayloadType*, size_t, size_t>
    void traverse_depthfirst(
        F&& func) const
    {
        traverse_depthfirst_impl(
            *this,
            std::forward<F>(func));
    }

    /// @brief Traverse in depth-first order (non-const overload)
    /// @tparam F Callable type, 
    ///         invocable as void(const PayloadType*, const PayloadType*, size_t, size_t).
    /// @param func Callable with signature 
    ///        void(const PayloadType* node, 
    ///             const PayloadType* parent, 
    ///             size_t node_index, 
    ///             size_t parent_index).
    /// @note The tree is optimized for depth-first traversal.
    template<class F>
        requires std::invocable<F, PayloadType*, PayloadType*, size_t, size_t>
    void traverse_depthfirst(
        F&& func)
    {
        traverse_depthfirst_impl(
            *this,
            std::forward<F>(func));
    }

    // --- Depth-first with level information (less fast) ---------------------

private:
    //–– by start‐index
    template<class T, class F>
    static void traverse_depthfirst_level_impl(
        T& self,
        size_t start_index,
        F&& func)
    {
        auto& nodes = self.nodes;
        if (!nodes.size()) return;
        // auto node_index = find_node_index(payload);
        assert(start_index != VecTree_NullIndex);
        assert(start_index >= 0);
        assert(start_index < nodes.size());

        std::vector<std::pair<size_t, size_t>> stack;
        stack.reserve(nodes[start_index].m_branch_stride);
        stack.push_back({ start_index, 0 });

        while (!stack.empty())
        {
            auto [index, level] = stack.back();
            stack.pop_back();

            auto& node = nodes[index];
            func(node.m_payload, index, level);

            std::vector<std::pair<size_t, size_t>> child_stack;
            stack.reserve(node.m_nbr_children);
            size_t child_index = index + 1;
            for (int i = 0; i < node.m_nbr_children; i++)
            {
                child_stack.push_back({ child_index, level + 1 });
                child_index += nodes[child_index].m_branch_stride;
            }
            stack.insert(
                stack.end(),
                child_stack.rbegin(),
                child_stack.rend());
        }
    }

    //–– by start‐payload
    template<class T, class F>
    static void traverse_depthfirst_level_impl(
        T& self,
        const PayloadType& start_payload,
        F&& func)
    {
        traverse_depthfirst_level_impl(
            self,
            self.find_node_index(start_payload),
            std::forward<F>(func));
    }

    //–– entire forest (all roots)
    template<class T, class F>
    static void traverse_depthfirst_level_impl(
        T& self,
        F&& func)
    {
        size_t i = 0;
        while (i < self.size())
        {
            traverse_depthfirst_level_impl(
                self,
                i,
                std::forward<F>(func));
            i += self.nodes[i].m_branch_stride;
        }
    }

public:
    /// @brief Traverse in depth-first order with level information (const overload)
    /// @tparam F Callable type, invocable as void(const PayloadType& payload, size_t, size_t).
    /// @param start_index Index of the node to start traversal from.
    /// @param func Callable with signature void(const PayloadType& payload, size_t index, size_t level).
    /// @note Level‑info version uses an explicit stack, incurring slight overhead.
    template<class F> requires std::invocable<F, const PayloadType&, size_t, size_t>
    void traverse_depthfirst(
        size_t start_index,
        F&& func) const
    {
        traverse_depthfirst_level_impl(
            *this,
            start_index,
            std::forward<F>(func));
    }

    /// @brief Traverse in depth-first order with level information
    /// @tparam F Callable type, invocable as void(PayloadType& payload, size_t, size_t).
    /// @param start_index Index of the node to start traversal from.
    /// @param func Callable with signature void(PayloadType& payload, size_t index, size_t level).
    /// @note Level‑info version uses an explicit stack, incurring slight overhead.
    template<class F> requires std::invocable<F, PayloadType&, size_t, size_t>
    void traverse_depthfirst(
        size_t start_index,
        F&& func)
    {
        traverse_depthfirst_level_impl(
            *this,
            start_index,
            std::forward<F>(func));
    }

    /// @brief Traverse in depth-first order with level information (const overload)
    /// @tparam F Callable type, invocable as void(const PayloadType& payload, size_t, size_t).
    /// @param start_payload Node to start traversal from.
    /// @param func Callable with signature void(const PayloadType& payload, size_t index, size_t level).
    /// @note Level‑info version uses an explicit stack, incurring slight overhead.
    template<class F> requires std::invocable<F, const PayloadType&, size_t, size_t>
    void traverse_depthfirst(
        const PayloadType& start_payload,
        F&& func) const
    {
        traverse_depthfirst_level_impl(
            *this,
            start_payload,
            std::forward<F>(func));
    }

    /// @brief Traverse in depth-first order with level information
    /// @tparam F Callable type, invocable as void(PayloadType& payload, size_t, size_t).
    /// @param start_payload Node to start traversal from.
    /// @param func Callable with signature void(PayloadType& payload, size_t index, size_t level).
    /// @note Level‑info version uses an explicit stack, incurring slight overhead.
    template<class F> requires std::invocable<F, PayloadType&, size_t, size_t>
    void traverse_depthfirst(
        const PayloadType& start_payload,
        F&& func)
    {
        traverse_depthfirst_level_impl(
            *this,
            start_payload,
            std::forward<F>(func));
    }

    /// @brief Traverse in depth-first order with level information (const overload)
    /// @tparam F Callable type, invocable as void(const PayloadType& payload, size_t, size_t).
    /// @param func Callable with signature void(const PayloadType& payload, size_t index, size_t level).
    /// @note Level‑info version uses an explicit stack, incurring slight overhead.
    template<class F> requires std::invocable<F, const PayloadType&, size_t, size_t>
    void traverse_depthfirst(
        F&& func) const
    {
        traverse_depthfirst_level_impl(
            *this,
            std::forward<F>(func));
    }

    /// @brief Traverse in depth-first order with level information
    /// @tparam F Callable type, invocable as void(PayloadType& payload, size_t, size_t).
    /// @param func Callable with signature void(PayloadType& payload, size_t index, size_t level).
    /// @note Level‑info version uses an explicit stack, incurring slight overhead.
    template<class F> requires std::invocable<F, PayloadType&, size_t, size_t>
    void traverse_depthfirst(
        F&& func)
    {
        traverse_depthfirst_level_impl(
            *this,
            std::forward<F>(func));
    }

    // --- Breadth-first ------------------------------------------------------

private:
    //–– by start‐index
    template<class T, class F>
    static void traverse_breadthfirst_impl(
        T& self,
        size_t start_index,
        F&& func)
    {
        auto& nodes = self.nodes;
        if (nodes.empty()) return;

        assert(start_index != VecTree_NullIndex);
        assert(start_index < nodes.size());

        std::queue<size_t> q;
        q.push(start_index);

        while (!q.empty())
        {
            size_t idx = q.front(); q.pop();
            auto& node = nodes[idx];
            func(node.m_payload, idx);

            size_t child = idx + 1;
            for (int i = 0; i < node.m_nbr_children; ++i)
            {
                q.push(child);
                child += nodes[child].m_branch_stride;
            }
        }
    }

    //–– by start‐payload
    template<class T, class F>
    static void traverse_breadthfirst_impl(
        T& self,
        const PayloadType& start_payload,
        F&& func)
    {
        auto idx = self.find_node_index(start_payload);
        traverse_breadthfirst_impl(
            self,
            idx,
            std::forward<F>(func));
    }

    //–– entire forest (all roots)
    template<class T, class F>
    static void traverse_breadthfirst_impl(
        T& self,
        F&& func)
    {
        size_t i = 0;
        while (i < self.nodes.size())
        {
            traverse_breadthfirst_impl(
                self,
                i,
                std::forward<F>(func));
            i += self.nodes[i].m_branch_stride;
        }
    }

public:
    /// @brief Traverse in breadth‑first order (const overload)
    /// @tparam F Callable type, invocable as void(const PayloadType& payload, size_t).
    /// @param start_index Index of the node to start traversal from.
    /// @param func Callable with signature void(const PayloadType& payload, size_t index).
    /// @note The tree is not optimized for breadth‑first traversal.
    template<class F>
        requires std::invocable<F, const PayloadType&, size_t>
    void traverse_breadthfirst(size_t start_index, F&& func) const
    {
        traverse_breadthfirst_impl(
            *this,
            start_index,
            std::forward<F>(func));
    }

    /// @brief Traverse in breadth‑first order
    /// @tparam F Callable type, invocable as void(PayloadType& payload, size_t).
    /// @param start_index Index of the node to start traversal from.
    /// @param func Callable with signature void(PayloadType& payload, size_t index).
    /// @note The tree is not optimized for breadth‑first traversal.
    template<class F>
        requires std::invocable<F, PayloadType&, size_t>
    void traverse_breadthfirst(size_t start_index, F&& func)
    {
        traverse_breadthfirst_impl(
            *this,
            start_index,
            std::forward<F>(func));
    }

    /// @brief Traverse in breadth‑first order (const overload)
    /// @tparam F Callable type, invocable as void(const PayloadType& payload, size_t).
    /// @param start_payload Node to start traversal from.
    /// @param func Callable with signature void(const PayloadType& payload, size_t index).
    /// @note The tree is not optimized for breadth‑first traversal.
    template<class F>
        requires std::invocable<F, const PayloadType&, size_t>
    void traverse_breadthfirst(const PayloadType& start_payload, F&& func) const
    {
        traverse_breadthfirst_impl(
            *this,
            start_payload,
            std::forward<F>(func));
    }

    /// @brief Traverse in breadth‑first order
    /// @tparam F Callable type, invocable as void(PayloadType& payload, size_t).
    /// @param start_payload Node to start traversal from.
    /// @param func Callable with signature void(PayloadType& payload, size_t index).
    /// @note The tree is not optimized for breadth‑first traversal.
    template<class F>
        requires std::invocable<F, PayloadType&, size_t>
    void traverse_breadthfirst(const PayloadType& start_payload, F&& func)
    {
        traverse_breadthfirst_impl(
            *this,
            start_payload,
            std::forward<F>(func));
    }

    /// @brief Traverse in breadth‑first order (const overload)
    /// @tparam F Callable type, invocable as void(const PayloadType& payload, size_t).
    /// @param func Callable with signature void(const PayloadType& payload, size_t index).
    /// @note The tree is not optimized for breadth‑first traversal.
    template<class F>
        requires std::invocable<F, const PayloadType&, size_t>
    void traverse_breadthfirst(F&& func) const
    {
        traverse_breadthfirst_impl(
            *this,
            std::forward<F>(func));
    }

    /// @brief Traverse in breadth‑first order
    /// @tparam F Callable type, invocable as void(PayloadType& payload, size_t).
    /// @param func Callable with signature void(PayloadType& payload, size_t index).
    /// @note The tree is not optimized for breadth‑first traversal.
    template<class F>
        requires std::invocable<F, PayloadType&, size_t>
    void traverse_breadthfirst(F&& func)
    {
        traverse_breadthfirst_impl(
            *this,
            std::forward<F>(func));
    }


    // --- Ascend -------------------------------------------------------------

    /// @brief Ascend to root.
    /// @param node_name Name of node to ascend from
    /// F is a function of type void(NodeType&, size_t), where the second argument is node index
    template<class F>
        requires std::invocable<F, PayloadType&, size_t>
    void ascend(
        size_t start_index,
        const F& func)
    {
        if (!nodes.size()) return;
        // auto node_index = find_node_index(payload);
        assert(start_index != VecTree_NullIndex);
        assert(start_index >= 0);
        assert(start_index < nodes.size());
        auto node_index = start_index; // find_node_index(node_name);

        while (nodes[node_index].m_parent_ofs)
        {
            func(nodes[node_index].m_payload, node_index);
            node_index -= nodes[node_index].m_parent_ofs;
        }
        func(nodes[node_index].m_payload, node_index);
    }

    template<class F>
        requires std::invocable<F, PayloadType&, size_t>
    void ascend(
        const PayloadType& start_payload,
        const F& func)
    {
        ascend(find_node_index(start_payload), func);
    }
};

#endif /* seqtree_h */

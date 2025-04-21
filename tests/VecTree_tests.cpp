#include "VecTree.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <set>

using TreeDesc = std::vector<std::pair<std::string, std::string>>;

namespace
{
    // Named tree descriptions for reusable test configurations
    const std::map<std::string, TreeDesc> test_trees = {
        {"SingleRoot", { {"A", ""} }},
        {"FlatTree", { {"A", ""}, {"B", "A"}, {"C", "A"}, {"D", "A"} }},
        {"LinearChain", { {"A", ""}, {"B", "A"}, {"C", "B"}, {"D", "C"} }},
        {"Balanced", { {"A", ""}, {"B", "A"}, {"C", "A"}, {"D", "B"}, {"E", "B"}, {"F", "C"}, {"G", "C"} }},
        {"MultiRoot", { {"A", ""}, {"B", ""}, {"C", ""} }}
    };

    const std::map<std::string, std::vector<std::pair<std::string, std::string>>> depthfirst_constraints = {
        {"LinearChain", { {"A", "B"}, {"B", "C"}, {"C", "D"} }},
        {"Balanced", {
            {"A", "B"}, {"A", "C"},
            {"B", "D"}, {"B", "E"},
            {"C", "F"}, {"C", "G"}
        }}
    };

    const std::map<std::string, std::vector<std::pair<std::string, std::string>>> breadthfirst_constraints = {
        {"FlatTree", {
            {"A", "B"}, {"A", "C"}, {"A", "D"}
        }},
        {"Balanced", {
            {"A", "B"}, {"A", "C"},
            {"B", "D"}, {"B", "E"},
            {"C", "F"}, {"C", "G"}
        }}
    };

    VecTree<std::string> BuildTree(const TreeDesc& desc) {
        VecTree<std::string> tree;
        for (const auto& [child, parent] : desc) {
            if (parent.empty()) tree.insert_as_root(child);
            else EXPECT_TRUE(tree.insert(child, parent));
        }
        return tree;
    }

    void PrintTree(const std::string& name, const VecTree<std::string>& tree) {
        std::cout << "--- Tree: " << name << " ---\n";
        tree.traverse_depthfirst([&](const std::string& payload, size_t idx, size_t level)
            {

                auto [nbr_children, branch_stride, parent_ofs] = tree.get_node_info(payload);
                std::cout
                    << std::string(level * 2, ' ') << "- " << payload
                    << " (children " << nbr_children
                    << ", stride " << branch_stride
                    << ", parent ofs " << parent_ofs << ")\n";
            });
    }

    void VerifyOrder(const std::vector<std::string>& order,
        const std::vector<std::pair<std::string, std::string>>& constraints,
        const std::string& tree_name) {
        for (const auto& [before, after] : constraints) {
            auto it_before = std::find(order.begin(), order.end(), before);
            auto it_after = std::find(order.begin(), order.end(), after);
            EXPECT_LT(it_before, it_after) << "Expected '" << before << "' before '" << after << "' in tree " << tree_name;
        }
    }

    // template<class T, class S>
    // void dump_tree_to_stream(
    //     const VecTree<T>& tree,
    //     S& outstream)
    // {
    //     tree.traverse_depthfirst([&](const T& node, size_t index, size_t level)
    //         {
    //             auto [nbr_children, branch_stride, parent_ofs] = tree.get_node_info(node);

    //             for (int i = 0; i < level; i++) outstream << "  ";
    //             outstream << node
    //                 << " (children " << nbr_children
    //                 << ", stride " << branch_stride
    //                 << ", parent ofs " << parent_ofs << ")\n";
    //         });
    // }
}

TEST(VecTreeBasicTest, EmptyTree) {
    VecTree<std::string> tree;
    EXPECT_EQ(tree.size(), 0u);
    EXPECT_FALSE(tree.contains("A"));
}

TEST(VecTreeBasicTest, InsertAndContains) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    EXPECT_EQ(tree.size(), 1u);
    EXPECT_TRUE(tree.contains("A"));
    EXPECT_TRUE(tree.is_root("A"));
    EXPECT_TRUE(tree.is_leaf("A"));
}

TEST(VecTreeInsertTest, InsertChildren) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    EXPECT_TRUE(tree.insert("B", "A"));
    EXPECT_TRUE(tree.insert("C", "A"));
    EXPECT_EQ(tree.size(), 3u);

    auto [nbrA, bsA, poA] = tree.get_node_info("A");
    EXPECT_EQ(nbrA, 2u);
    EXPECT_EQ(tree.get_nbr_children("A"), 2u);
    EXPECT_EQ(tree.get_branch_size("A"), 3u);
    EXPECT_FALSE(tree.is_leaf("A"));

    EXPECT_EQ(tree.get_parent("B"), "A");
    EXPECT_EQ(tree.get_parent("C"), "A");
    EXPECT_TRUE(tree.is_leaf("B"));
    EXPECT_TRUE(tree.is_leaf("C"));
}

TEST(VecTreeNestedTest, NestedInsertionAndRelationships) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    EXPECT_EQ(tree.size(), 4u);
    EXPECT_EQ(tree.get_nbr_children("B"), 1u);
    EXPECT_EQ(tree.get_branch_size("B"), 2u);
    EXPECT_EQ(tree.get_branch_size("A"), 4u);

    EXPECT_TRUE(tree.is_descendant_of("D", "A"));
    EXPECT_TRUE(tree.is_descendant_of("D", "B"));
    EXPECT_FALSE(tree.is_descendant_of("C", "B"));
    EXPECT_EQ(tree.get_parent("D"), "B");
}

TEST(VecTreeEraseTest, EraseBranch) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    EXPECT_TRUE(tree.erase_branch("B"));
    EXPECT_EQ(tree.size(), 2u);
    EXPECT_FALSE(tree.contains("B"));
    EXPECT_FALSE(tree.contains("D"));
    EXPECT_EQ(tree.get_nbr_children("A"), 1u);
    EXPECT_EQ(tree.get_branch_size("A"), 2u);
}

TEST(VecTreeEraseTest, EraseRootBranch) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    EXPECT_EQ(tree.size(), 4u);
    EXPECT_EQ(tree.get_nbr_children("A"), 2u);
    EXPECT_EQ(tree.get_branch_size("A"), 4u);

    EXPECT_TRUE(tree.erase_branch("A"));

    EXPECT_EQ(tree.size(), 0u);
    EXPECT_FALSE(tree.contains("A"));
    EXPECT_FALSE(tree.contains("B"));
    EXPECT_FALSE(tree.contains("C"));
}

TEST(VecTreeReparentTest, ReparentNode) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    // Move C under B
    tree.reparent("C", "B");
    EXPECT_EQ(tree.get_nbr_children("A"), 1u);
    EXPECT_EQ(tree.get_nbr_children("B"), 2u);
    EXPECT_TRUE(tree.is_descendant_of("C", "B"));
    EXPECT_EQ(tree.get_parent("C"), "B");
}

TEST(VecTreeUnparentTest, UnparentNode) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "B");

    EXPECT_EQ(tree.get_nbr_children("B"), 1u);
    tree.unparent("C");
    EXPECT_TRUE(tree.is_root("C"));
    EXPECT_FALSE(tree.is_descendant_of("C", "A"));
}

TEST(VecTreeTraversalTest, DepthFirstTraversal) {
    for (const auto& [name, desc] : test_trees) {
        SCOPED_TRACE("Tree: " + name);
        VecTree<std::string> tree = BuildTree(desc);
        PrintTree(name, tree);

        std::vector<std::string> order;
        tree.traverse_depthfirst([&](std::string& payload, size_t idx) {
            order.push_back(payload);
            });

        std::set<std::string> expected;
        for (auto& [n, _] : desc) expected.insert(n);
        std::set<std::string> actual(order.begin(), order.end());
        EXPECT_EQ(actual, expected);

        if (depthfirst_constraints.contains(name)) {
            VerifyOrder(order, depthfirst_constraints.at(name), name);
        }
    }
}

// Breadth-first traversal: verifies that all nodes are visited and that the root appears first.
// No specific order among siblings is assumed.
TEST(VecTreeTraversalTest, BreadthFirstTraversal) 
{
    for (const auto& [name, desc] : test_trees) {
        SCOPED_TRACE("Tree: " + name);
        const VecTree<std::string> tree = BuildTree(desc);
        PrintTree(name, tree);

        std::vector<std::string> order;
        tree.traverse_breadthfirst([&](const std::string& payload, size_t idx) {
            order.push_back(payload);
        });

        std::set<std::string> expected;
        for (auto& [n, _] : desc) expected.insert(n);
        std::set<std::string> actual(order.begin(), order.end());
        EXPECT_EQ(actual, expected);

        if (!order.empty()) {
            EXPECT_TRUE(tree.is_root(order.front()));
        }

        if (breadthfirst_constraints.contains(name)) {
            VerifyOrder(order, breadthfirst_constraints.at(name), name);
        }
    }
}

#if 0
TEST(VecTreeTraversalTest, AscendTraversal) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "B");

    vector<std::string> order;
    tree.ascend("C", [&](std::string& payload, size_t idx) {
        order.push_back(payload);
        });

    vector<std::string> expected = { "C", "B", "A" };
    EXPECT_EQ(order, expected);
}

TEST(VecTreeTraversalTest, DepthFirstWithLevel) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    vector<pair<std::string, size_t>> results;
    tree.traverse_depthfirst([&](std::string& payload, size_t idx, size_t level) {
        results.emplace_back(payload, level);
        });

    vector<pair<std::string, size_t>> expected = { {"A", 0}, {"B", 1}, {"D", 2}, {"C", 1} };
    EXPECT_EQ(results, expected);
}

TEST(VecTreeTraversalTest, ProgressiveTraversal) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    vector<pair<std::string, std::string>> calls;
    tree.traverse_progressive([&](std::string* node, std::string* parent, size_t idx, size_t pidx) {
        calls.emplace_back(
            node ? *node : std::string(""),
            parent ? *parent : std::string("(null)")
        );
        });

    vector<pair<std::string, std::string>> expected = {
        {"A", "(null)"},
        {"B", "A"},
        {"C", "A"},
        {"D", "B"}
    };
    EXPECT_EQ(calls, expected);
}
#endif

// Test erasing a leaf and adjusting sibling offsets
TEST(VecTreeEraseSiblingTest, EraseSiblingAndAdjustOffsets) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    EXPECT_EQ(tree.get_nbr_children("A"), 2u);
    EXPECT_EQ(tree.get_branch_size("A"), 3u);
    EXPECT_TRUE(tree.erase_branch("B"));
    EXPECT_FALSE(tree.contains("B"));
    EXPECT_TRUE(tree.contains("C"));
    EXPECT_EQ(tree.get_nbr_children("A"), 1u);
    EXPECT_EQ(tree.get_branch_size("A"), 2u);
    auto [nbrC, bsC, parentOfsC] = tree.get_node_info("C");
    EXPECT_EQ(parentOfsC, 1u);
}

// Test deep-tree reparenting
TEST(VecTreeReparentDeepTest, ReparentMidSubtree) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "B");
    tree.insert("D", "C");
    EXPECT_EQ(tree.get_branch_size("A"), 4u);
    EXPECT_EQ(tree.get_branch_size("B"), 3u);
    EXPECT_EQ(tree.get_branch_size("C"), 2u);
    tree.reparent("C", "A");
    EXPECT_EQ(tree.get_nbr_children("A"), 2u);
    EXPECT_EQ(tree.get_branch_size("A"), 4u);
    EXPECT_EQ(tree.get_branch_size("B"), 1u);
    EXPECT_TRUE(tree.is_leaf("B"));
    EXPECT_EQ(tree.get_nbr_children("C"), 1u);
    EXPECT_EQ(tree.get_parent("C"), "A");
    EXPECT_EQ(tree.get_parent("D"), "C");
    EXPECT_TRUE(tree.is_leaf("D"));
}

// main() can be omitted if linked with GTest's main library

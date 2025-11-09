#include <gtest/gtest.h>

#include "util/scope_hash_set.hpp"

class ScopeHashSetTest : public ::testing::Test {
   protected:
    void SetUp() override { test_set = std::make_unique<ScopeHashSet<int>>(); }

    std::unique_ptr<ScopeHashSet<int>> test_set;
};

// ========== Basic Operations Tests ==========

TEST_F(ScopeHashSetTest, DefaultConstructor) {
    EXPECT_TRUE(test_set->empty());
    EXPECT_EQ(test_set->size(), 0);
}

TEST_F(ScopeHashSetTest, CustomCapacityConstructor) {
    ScopeHashSet<int> set(1024);
    EXPECT_EQ(set.size(), 0);
    EXPECT_GE(set.bucket_count(), 1024);
}

TEST_F(ScopeHashSetTest, SingleInsertion) {
    test_set->insert(42);
    EXPECT_FALSE(test_set->empty());
    EXPECT_EQ(test_set->size(), 1);
    EXPECT_NE(test_set->find(42), test_set->end());
}

TEST_F(ScopeHashSetTest, MultipleInsertions) {
    for (int i = 0; i < 100; ++i) {
        test_set->insert(i);
    }
    EXPECT_EQ(test_set->size(), 100);
    for (int i = 0; i < 100; ++i) {
        EXPECT_NE(test_set->find(i), test_set->end());
    }
}

// ========== Scope Management Tests ==========

TEST_F(ScopeHashSetTest, SingleScopePushPop) {
    auto marker = test_set->get_scope_marker();
    test_set->insert(1);
    test_set->insert(2);
    EXPECT_EQ(test_set->size(), 2);

    test_set->pop_scope(marker);
    EXPECT_EQ(test_set->size(), 0);
    EXPECT_EQ(test_set->find(1), test_set->end());
    EXPECT_EQ(test_set->find(2), test_set->end());
}

TEST_F(ScopeHashSetTest, NestedScopes) {
    // Global scope
    test_set->insert(0);

    // Scope 1
    auto marker1 = test_set->get_scope_marker();
    test_set->insert(1);
    test_set->insert(2);

    // Scope 2
    auto marker2 = test_set->get_scope_marker();
    test_set->insert(3);
    test_set->insert(4);

    EXPECT_EQ(test_set->size(), 5);

    // Pop scope 2
    test_set->pop_scope(marker2);
    EXPECT_EQ(test_set->size(), 3);
    EXPECT_EQ(test_set->find(0), test_set->begin());
    EXPECT_NE(test_set->find(1), test_set->end());
    EXPECT_NE(test_set->find(2), test_set->end());
    EXPECT_EQ(test_set->find(3), test_set->end());
    EXPECT_EQ(test_set->find(4), test_set->end());

    // Pop scope 1
    test_set->pop_scope(marker1);
    EXPECT_EQ(test_set->size(), 1);
    EXPECT_NE(test_set->find(0), test_set->end());
    EXPECT_EQ(test_set->find(1), test_set->end());
}

// ========== Iterator Tests ==========

TEST_F(ScopeHashSetTest, IteratorTraversal) {
    test_set->insert(10);
    test_set->insert(20);
    test_set->insert(30);

    std::vector<int> values;
    for (auto it : *test_set) {
        values.push_back(it);
    }

    EXPECT_EQ(values.size(), 3);
    // Values should appear in insertion order due to scope chain
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
    EXPECT_EQ(values[2], 30);
}

TEST_F(ScopeHashSetTest, IteratorAfterScopePop) {
    auto marker = test_set->get_scope_marker();
    test_set->insert(1);
    test_set->insert(2);

    test_set->pop_scope(marker);

    // Should have no elements to iterate
    EXPECT_EQ(test_set->begin(), test_set->end());
}

TEST_F(ScopeHashSetTest, EmptyIterator) {
    EXPECT_EQ(test_set->begin(), test_set->end());
}

// ========== Find & Equal Range Tests ==========

TEST_F(ScopeHashSetTest, FindExistingElement) {
    test_set->insert(42);
    test_set->insert(43);

    auto it = test_set->find(42);
    EXPECT_NE(it, test_set->end());
    EXPECT_EQ(*it, 42);
}

TEST_F(ScopeHashSetTest, FindNonExistingElement) {
    test_set->insert(42);
    EXPECT_EQ(test_set->find(43), test_set->end());
}

TEST_F(ScopeHashSetTest, EqualRangeWithHashCollisions) {
    // Use custom hash that always returns same bucket
    struct BadHash {
        size_t operator()(int) const { return 0; }
    };

    ScopeHashSet<int, BadHash> set;
    set.insert(1);
    set.insert(2);
    set.insert(3);

    auto range = set.equal_range(2);
    auto it = range.begin();
    EXPECT_NE(it, range.end());
    EXPECT_EQ(*it, 2);
    ++it;
    // Should reach end after the matching element
    EXPECT_EQ(it, range.end());
}

// ========== Rehash Tests ==========

TEST_F(ScopeHashSetTest, ManualRehash) {
    for (int i = 0; i < 100; ++i) {
        test_set->insert(i);
    }

    size_t old_capacity = test_set->bucket_count();
    test_set->rehash(old_capacity * 4);

    EXPECT_EQ(test_set->bucket_count(), old_capacity * 4);
    EXPECT_EQ(test_set->size(), 100);

    // Verify elements are still accessible
    for (int i = 0; i < 100; ++i) {
        EXPECT_NE(test_set->find(i), test_set->end());
    }
}

TEST_F(ScopeHashSetTest, AutomaticRehashOnLoadFactor) {
    ScopeHashSet<int> set(16);  // Small capacity to trigger rehash

    // Insert enough elements to exceed load factor (16 * 0.75 = 12)
    for (int i = 0; i < 20; ++i) {
        set.insert(i);
    }

    // Should have grown automatically
    EXPECT_GT(set.bucket_count(), 16);
    EXPECT_EQ(set.size(), 20);
}

// ========== Move Semantics Tests ==========

TEST_F(ScopeHashSetTest, MoveConstructor) {
    test_set->insert(1);
    test_set->insert(2);

    ScopeHashSet<int> moved_set(std::move(*test_set));

    EXPECT_EQ(moved_set.size(), 2);
    EXPECT_NE(moved_set.find(1), moved_set.end());
    EXPECT_NE(moved_set.find(2), moved_set.end());

    // Source should be empty
    EXPECT_EQ(test_set->size(), 0);
    // EXPECT_EQ(test_set->head, nullptr);
}

TEST_F(ScopeHashSetTest, MoveAssignment) {
    test_set->insert(1);
    test_set->insert(2);

    ScopeHashSet<int> assigned_set;
    assigned_set = std::move(*test_set);

    EXPECT_EQ(assigned_set.size(), 2);
    EXPECT_NE(assigned_set.find(1), assigned_set.end());
    EXPECT_NE(assigned_set.find(2), assigned_set.end());

    EXPECT_EQ(test_set->size(), 0);
}

// TEST_F(ScopeHashSetTest, SelfMoveAssignment) {
//     test_set->insert(42);
//     *test_set =
//         std::move(*test_set);  // Should handle self-assignment gracefully
//     EXPECT_EQ(test_set->size(), 1);
//     EXPECT_NE(test_set->find(42), test_set->end());
// }

// ========== Edge Cases & Stress Tests ==========

TEST_F(ScopeHashSetTest, InsertDuplicates) {
    test_set->insert(42);
    test_set->insert(42);
    EXPECT_EQ(test_set->size(), 2);  // Current implementation allows duplicates
    EXPECT_NE(test_set->find(42), test_set->end());
}

TEST_F(ScopeHashSetTest, LargeCapacity) {
    ScopeHashSet<int> set(1000000);
    set.insert(1);
    EXPECT_EQ(set.size(), 1);
    EXPECT_NE(set.find(1), set.end());
}

TEST_F(ScopeHashSetTest, CustomHashAndPred) {
    struct CustomHash {
        size_t operator()(const std::string& s) const {
            return std::hash<std::string>{}(s) ^ 0xDEADBEEF;
        }
        size_t operator()(const std::string_view& s) const {
            return std::hash<std::string_view>{}(s) ^ 0xDEADBEEF;
        }
    };
    struct CustomPred {
        bool operator()(const std::string& a, const std::string& b) const {
            return a == b;
        }
        bool operator()(const std::string& a, const std::string_view& b) const {
            return a == b;
        }
        bool operator()(const std::string_view& a, const std::string b) const {
            return a == b;
        }
    };

    using namespace std::string_view_literals;
    ScopeHashSet<std::string, CustomHash, CustomPred> set;
    set.insert("hello");
    set.insert("world");

    EXPECT_NE(set.find("hello"sv), set.end());
    EXPECT_NE(set.find("world"sv), set.end());
    EXPECT_EQ(set.find("foo"sv), set.end());
}

// ========== Iterator Stability Tests ==========

TEST_F(ScopeHashSetTest, IteratorValidityAfterRehash) {
    for (int i = 0; i < 50; ++i) {
        test_set->insert(i);
    }

    // Trigger rehash (invalidates all iterators)
    test_set->rehash(1000);

    // New iteration should work correctly after rehash
    std::vector<int> values;
    for (auto it = test_set->begin(); it != test_set->end(); ++it) {
        values.push_back(*it);
    }
    EXPECT_EQ(values.size(), 50);
}

// ========== Integration Tests ==========

TEST_F(ScopeHashSetTest, SymbolTableSimulation) {
    // Simulate nested scopes like a compiler symbol table
    ScopeHashSet<std::string> symbol_table;

    // Global scope
    symbol_table.insert("global_var");

    // Function scope
    auto func_scope = symbol_table.get_scope_marker();
    symbol_table.insert("local_var");
    symbol_table.insert("param");

    // Block scope
    auto block_scope = symbol_table.get_scope_marker();
    symbol_table.insert("temp_var");

    EXPECT_EQ(symbol_table.size(), 4);

    // Exit block scope
    symbol_table.pop_scope(block_scope);
    EXPECT_EQ(symbol_table.find("temp_var"), symbol_table.end());
    EXPECT_EQ(symbol_table.size(), 3);

    // Exit function scope
    symbol_table.pop_scope(func_scope);
    EXPECT_EQ(symbol_table.find("local_var"), symbol_table.end());
    EXPECT_EQ(symbol_table.find("param"), symbol_table.end());
    EXPECT_EQ(symbol_table.size(), 1);

    // Global still exists
    EXPECT_NE(symbol_table.find("global_var"), symbol_table.end());
}
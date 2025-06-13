#include <gtest/gtest.h>
#include <thread>
#include <atomic>

#include "PoolAllocatorTFH.h"
#include "Handle.h"

using namespace eeng;

namespace {
    // Test struct for move semantics
    struct MoveTest {
        static std::atomic<int> constructions;
        static std::atomic<int> destructions;
        int value;
        char padding[sizeof(std::size_t)]; // Ensure object is large enough for freelist

        MoveTest(int val) : value(val) { ++constructions; }
        MoveTest(const MoveTest&) = delete;
        MoveTest(MoveTest&& other) noexcept : value(other.value) { other.value = -1; ++constructions; }
        ~MoveTest() { ++destructions; }

        static void reset() {
            constructions.store(0, std::memory_order_relaxed);
            destructions.store(0, std::memory_order_relaxed);
        }
    };

    std::atomic<int> MoveTest::constructions = 0;
    std::atomic<int> MoveTest::destructions = 0;

    static bool is_aligned(void* ptr, std::size_t align)
    {
        return (reinterpret_cast<uintptr_t>(ptr) % align) == 0;
    }
}

// Fixture
class PoolAllocatorTFHTest : public ::testing::Test
{
protected:
    PoolAllocatorTFH<MoveTest> pool;

    void SetUp() override {
        MoveTest::reset();
    }
};

TEST_F(PoolAllocatorTFHTest, InitialCapacityIsZero)
{
    EXPECT_EQ(pool.capacity(), 0u);
}

TEST_F(PoolAllocatorTFHTest, CreateSingleElement)
{
    auto handle = pool.create(42);
    auto& elem = pool.get(handle);
    elem.value = 43;
    EXPECT_EQ(elem.value, 43);
    EXPECT_EQ(MoveTest::constructions, 1);

    // const auto& cpool = pool;
    // MoveTest& celem = cpool.get(handle);
    // auto& celem = cpool.get(handle); celem.value = 42;
    // EXPECT_EQ(celem.value, 42);

}

TEST_F(PoolAllocatorTFHTest, DestroyElement)
{
    auto handle = pool.create(10);
    pool.destroy(handle);

    EXPECT_EQ(MoveTest::constructions, 1);
    EXPECT_EQ(MoveTest::destructions, 1);
}

TEST_F(PoolAllocatorTFHTest, PoolExpandsWhenFull)
{
    const int elements_to_create = 50;  // Ensure pool expansion

    std::vector<Handle<MoveTest>> handles;
    for (int i = 0; i < elements_to_create; ++i)
        handles.push_back(pool.create(i));

    EXPECT_GE(pool.capacity(), elements_to_create * sizeof(MoveTest));
    EXPECT_GE(MoveTest::constructions, elements_to_create);
    EXPECT_EQ(MoveTest::constructions - MoveTest::destructions, handles.size());

    for (int i = 0; i < elements_to_create; ++i)
        EXPECT_EQ(pool.get(handles[i]).value, i);
}

TEST_F(PoolAllocatorTFHTest, FreelistReuse)
{
    auto handle1 = pool.create(1);
    auto handle2 = pool.create(2);

    pool.destroy(handle1);

    auto handle3 = pool.create(3);

    // handle3 should reuse handle1's slot
    EXPECT_EQ(handle1.ofs, handle3.ofs);
}

TEST_F(PoolAllocatorTFHTest, MoveSemanticsOnExpansion)
{
    auto handle1 = pool.create(100);

    size_t initial_capacity = pool.capacity();

    // Trigger expansion
    std::vector<Handle<MoveTest>> more_handles;
    for (int i = 0; i < 100; ++i)
        more_handles.push_back(pool.create(i));

    EXPECT_GT(pool.capacity(), initial_capacity);
    EXPECT_EQ(pool.get(handle1).value, 100);
}

TEST_F(PoolAllocatorTFHTest, CountFree)
{
    EXPECT_EQ(pool.count_free(), 0);

    auto handle1 = pool.create(5);
    auto handle2 = pool.create(10);

    pool.destroy(handle1);
    EXPECT_EQ(pool.count_free(), 1);

    pool.destroy(handle2);
    EXPECT_EQ(pool.count_free(), 2);
}

TEST_F(PoolAllocatorTFHTest, UsedVisitor)
{
    auto handle1 = pool.create(7);
    auto handle2 = pool.create(14);

    pool.destroy(handle1);

    int sum = 0;
    pool.used_visitor([&](const MoveTest& mt) {
        sum += mt.value;
        });

    EXPECT_EQ(sum, 14);
}

TEST_F(PoolAllocatorTFHTest, ToStringIsNonEmptyAfterCreates)
{
    // Arrange: create a couple of elements
    pool.create(123);
    pool.create(456);

    // Act & Assert: should not throw, and the returned string must not be empty
    EXPECT_NO_THROW({
        auto s = pool.to_string();
        EXPECT_FALSE(s.empty()) << "to_string() should produce at least some output";
        });

    // {
    //     PoolAllocatorTFH<size_t> pool; // Test copy constructor
    //     pool.create(1);
    //     pool.create(2);
    //     pool.create(3);
    //     pool.create(4);
    //     pool.create(5);
    //     std::cout << pool.to_string() << std::endl;
    // }
}

TEST_F(PoolAllocatorTFHTest, ThreadSafetyCreateDestroy)
{
    const int thread_count = 8;
    const int iterations_per_thread = 1000;

    PoolAllocatorTFH<MoveTest> pool;
    MoveTest::reset();

    std::vector<std::thread> threads;

    for (int t = 0; t < thread_count; ++t)
    {
        threads.emplace_back([&]() {
            std::vector<Handle<MoveTest>> handles;
            for (int i = 0; i < iterations_per_thread; ++i)
            {
                auto h = pool.create(i);
                handles.push_back(h);
            }
            for (auto h : handles)
            {
                pool.destroy(h);
            }
            });
    }

    for (auto& thread : threads)
        thread.join();

    EXPECT_GE(MoveTest::constructions.load(), thread_count * iterations_per_thread);
    EXPECT_EQ(MoveTest::constructions.load(), MoveTest::destructions.load());

}

TEST_F(PoolAllocatorTFHTest, RespectsNaturalAlignment)
{
    // a type with natural alignment 64
    struct alignas(64) Aligned64 { int x; };

    // default Alignment = max(alignof(T), alignof(TIndex)) == 64
    PoolAllocatorTFH<Aligned64> pool(1);

    auto h = pool.create(Aligned64{ 42 });
    auto* p = &pool.get(h);

    EXPECT_TRUE(is_aligned(p, alignof(Aligned64)))
        << "pointer " << p << " must be aligned to " << alignof(Aligned64);
}

TEST_F(PoolAllocatorTFHTest, RespectsForced256Alignment)
{
    // natural alignment
    struct Tiny { size_t x; };

    // force 256-byte alignment
    PoolAllocatorTFH<Tiny, std::size_t, 256> pool(1);

    auto h = pool.create(Tiny{ 0 });
    auto* p = &pool.get(h);

    EXPECT_TRUE(is_aligned(p, 256))
        << "pointer " << p << " must be aligned to 256 bytes";
}
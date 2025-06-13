#include <gtest/gtest.h>
#include <thread>
#include <atomic>

#include "PoolAllocatorFH.h"
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
class PoolAllocatorFHTest : public ::testing::Test
{
protected:
    TypeInfo type_info = TypeInfo::create<MoveTest>();
    PoolAllocatorFH pool{ type_info, 16 };

    void SetUp() override {
        MoveTest::reset();
    }
};

TEST_F(PoolAllocatorFHTest, InitialCapacityIsZero)
{
    EXPECT_EQ(pool.capacity(), 0u);
}

TEST_F(PoolAllocatorFHTest, CreateSingleElement)
{
    auto handle = pool.create<MoveTest>(42);
    auto& elem = pool.get(handle);

    EXPECT_EQ(elem.value, 42);
    EXPECT_EQ(MoveTest::constructions, 1);
}

TEST_F(PoolAllocatorFHTest, DestroyElement)
{
    auto handle = pool.create<MoveTest>(10);
    pool.destroy(handle);

    EXPECT_EQ(MoveTest::constructions, 1);
    EXPECT_EQ(MoveTest::destructions, 1);
}

TEST_F(PoolAllocatorFHTest, PoolExpandsWhenFull)
{
    const int elements_to_create = 50;  // Ensure pool expansion

    std::vector<Handle<MoveTest>> handles;
    for (int i = 0; i < elements_to_create; ++i)
        handles.push_back(pool.create<MoveTest>(i));

    EXPECT_GE(pool.capacity(), elements_to_create * sizeof(MoveTest));
    EXPECT_GE(MoveTest::constructions, elements_to_create);
    EXPECT_EQ(MoveTest::constructions - MoveTest::destructions, handles.size());

    for (int i = 0; i < elements_to_create; ++i)
        EXPECT_EQ(pool.get(handles[i]).value, i);
}

TEST_F(PoolAllocatorFHTest, FreelistReuse)
{
    auto handle1 = pool.create<MoveTest>(1);
    auto handle2 = pool.create<MoveTest>(2);

    pool.destroy(handle1);

    auto handle3 = pool.create<MoveTest>(3);

    // handle3 should reuse handle1's slot
    EXPECT_EQ(handle1.ofs, handle3.ofs);
}

TEST_F(PoolAllocatorFHTest, MoveSemanticsOnExpansion)
{
    auto handle1 = pool.create<MoveTest>(100);

    size_t initial_capacity = pool.capacity();

    // Trigger expansion
    std::vector<Handle<MoveTest>> more_handles;
    for (int i = 0; i < 100; ++i)
        more_handles.push_back(pool.create<MoveTest>(i));

    EXPECT_GT(pool.capacity(), initial_capacity);
    EXPECT_EQ(pool.get(handle1).value, 100);
}

TEST_F(PoolAllocatorFHTest, CountFree)
{
    EXPECT_EQ(pool.count_free(), 0);

    auto handle1 = pool.create<MoveTest>(5);
    auto handle2 = pool.create<MoveTest>(10);

    pool.destroy(handle1);
    EXPECT_EQ(pool.count_free(), 1);

    pool.destroy(handle2);
    EXPECT_EQ(pool.count_free(), 2);
}

TEST_F(PoolAllocatorFHTest, UsedVisitor)
{
    auto handle1 = pool.create<MoveTest>(7);
    auto handle2 = pool.create<MoveTest>(14);

    pool.destroy(handle1);

    int sum = 0;
    pool.used_visitor<MoveTest>([&](const MoveTest& mt) {
        sum += mt.value;
        });

    EXPECT_EQ(sum, 14);
}

TEST_F(PoolAllocatorFHTest, DumpPoolDebug)
{
    // Just for visual/manual inspection; should not crash
    pool.create<MoveTest>(123);
    pool.create<MoveTest>(456);

    testing::internal::CaptureStdout();
    pool.dump_pool();
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(output.empty());
}

TEST_F(PoolAllocatorFHTest, TypeMismatchAssert)
{
#ifndef NDEBUG
    ASSERT_DEATH({ pool.create<int>(42); }, "");
#endif
}

TEST_F(PoolAllocatorFHTest, RejectsTooSmallType)
{
    struct Tiny { char x; };

#ifndef NDEBUG
    ASSERT_DEATH(
        [] {
            PoolAllocatorFH p(TypeInfo::create<Tiny>(), 16);
            (void)p;
        }(), "");
#endif
}

TEST_F(PoolAllocatorFHTest, ThreadSafetyCreateDestroy)
{
    const int thread_count = 8;
    const int iterations_per_thread = 1000;

    TypeInfo type_info = TypeInfo::create<MoveTest>();
    PoolAllocatorFH pool(type_info, 16);
    MoveTest::reset();

    std::vector<std::thread> threads;

    for (int t = 0; t < thread_count; ++t)
    {
        threads.emplace_back([&]() {
            std::vector<Handle<MoveTest>> handles;
            for (int i = 0; i < iterations_per_thread; ++i)
            {
                auto h = pool.create<MoveTest>(i);
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

TEST_F(PoolAllocatorFHTest, RespectsNaturalAlignment) 
{
    // a type with natural alignment 64
    struct alignas(64) Aligned64 { int x; };

    // construct with TypeInfo and desired alignment == alignof(Aligned64)
    PoolAllocatorFH pool(TypeInfo::create<Aligned64>(), /*alignment=*/ alignof(Aligned64));

    // allocate one element
    auto h = pool.create<Aligned64>(Aligned64{42});
    auto* p = &pool.get<Aligned64>(h);

    EXPECT_TRUE(is_aligned(p, alignof(Aligned64)))
        << "pointer " << p
        << " must be aligned to " << alignof(Aligned64);
}

// ─── Forced 256‐byte alignment ─────────────────────────────────────────────────

TEST_F(PoolAllocatorFHTest, RespectsForced256Alignment) 
{
    // a small type (natural alignment = 1) but force to 256
    struct Tiny { std::size_t x; };

    PoolAllocatorFH pool(TypeInfo::create<Tiny>(), /*alignment=*/ 256);

    auto h = pool.create<Tiny>(Tiny{0});
    auto* p = &pool.get<Tiny>(h);

    EXPECT_TRUE(is_aligned(p, 256))
        << "pointer " << p
        << " must be aligned to 256 bytes";
}
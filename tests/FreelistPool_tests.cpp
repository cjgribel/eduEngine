#include <gtest/gtest.h>
#include "FreelistPool.h"
#include "Handle.h"

using namespace eeng;

// Test struct for move semantics
struct MoveTest {
    static int constructions;
    static int destructions;
    int value;
    char padding[sizeof(std::size_t)]; // Ensure object is large enough for freelist

    MoveTest(int val) : value(val) { ++constructions; }
    MoveTest(const MoveTest&) = delete;
    MoveTest(MoveTest&& other) noexcept : value(other.value) { other.value = -1; ++constructions; }
    ~MoveTest() { ++destructions; }

    static void reset() { constructions = destructions = 0; }
};

int MoveTest::constructions = 0;
int MoveTest::destructions = 0;

// Fixture
class RawFreelistPoolTest : public ::testing::Test
{
protected:
    TypeInfo type_info = TypeInfo::create<MoveTest>();
    FreelistPool pool{type_info, 16};

    void SetUp() override {
        MoveTest::reset();
    }
};

TEST_F(RawFreelistPoolTest, InitialCapacityIsZero)
{
    EXPECT_EQ(pool.capacity(), 0u);
}

TEST_F(RawFreelistPoolTest, CreateSingleElement)
{
    auto handle = pool.create<MoveTest>(42);
    auto& elem = pool.get(handle);

    EXPECT_EQ(elem.value, 42);
    EXPECT_EQ(MoveTest::constructions, 1);
}

TEST_F(RawFreelistPoolTest, DestroyElement)
{
    auto handle = pool.create<MoveTest>(10);
    pool.destroy(handle);

    EXPECT_EQ(MoveTest::constructions, 1);
    EXPECT_EQ(MoveTest::destructions, 1);
}

TEST_F(RawFreelistPoolTest, PoolExpandsWhenFull)
{
    const int elements_to_create = 50;  // Ensure pool expansion

    std::vector<Handle<MoveTest>> handles;
    for(int i = 0; i < elements_to_create; ++i)
        handles.push_back(pool.create<MoveTest>(i));

    EXPECT_GE(pool.capacity(), elements_to_create * sizeof(MoveTest));
    EXPECT_GE(MoveTest::constructions, elements_to_create);
    EXPECT_EQ(MoveTest::constructions - MoveTest::destructions, handles.size());

    for(int i = 0; i < elements_to_create; ++i)
        EXPECT_EQ(pool.get(handles[i]).value, i);
}

TEST_F(RawFreelistPoolTest, FreelistReuse)
{
    auto handle1 = pool.create<MoveTest>(1);
    auto handle2 = pool.create<MoveTest>(2);

    pool.destroy(handle1);

    auto handle3 = pool.create<MoveTest>(3);

    // handle3 should reuse handle1's slot
    EXPECT_EQ(handle1.ofs, handle3.ofs);
}

TEST_F(RawFreelistPoolTest, MoveSemanticsOnExpansion)
{
    auto handle1 = pool.create<MoveTest>(100);

    size_t initial_capacity = pool.capacity();

    // Trigger expansion
    std::vector<Handle<MoveTest>> more_handles;
    for(int i = 0; i < 100; ++i)
        more_handles.push_back(pool.create<MoveTest>(i));

    EXPECT_GT(pool.capacity(), initial_capacity);
    EXPECT_EQ(pool.get(handle1).value, 100);
}

TEST_F(RawFreelistPoolTest, CountFree)
{
    EXPECT_EQ(pool.count_free(), 0);

    auto handle1 = pool.create<MoveTest>(5);
    auto handle2 = pool.create<MoveTest>(10);

    pool.destroy(handle1);
    EXPECT_EQ(pool.count_free(), 1);

    pool.destroy(handle2);
    EXPECT_EQ(pool.count_free(), 2);
}

TEST_F(RawFreelistPoolTest, UsedVisitor)
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

TEST_F(RawFreelistPoolTest, DumpPoolDebug)
{
    // Just for visual/manual inspection; should not crash
    pool.create<MoveTest>(123);
    pool.create<MoveTest>(456);

    testing::internal::CaptureStdout();
    pool.dump_pool();
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(output.empty());
}

TEST_F(RawFreelistPoolTest, TypeMismatchAssert)
{
#ifndef NDEBUG
    ASSERT_DEATH({ pool.create<int>(42); }, "");
#endif
}

TEST(FreelistPoolTest, RejectsTooSmallType)
{
    struct Tiny { char x; };

#ifndef NDEBUG
    ASSERT_DEATH(
        [] {
            FreelistPool p(TypeInfo::create<Tiny>(), 16);
            (void)p;
        }(), "");
#endif
}

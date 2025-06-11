#include "gtest/gtest.h"
#include "Guid.h"
#include "Handle.h"
//#include "resource_pool.hpp"
#include "ResourceRegistry.hpp"

using namespace eeng;

struct DummyResource
{
    size_t value = 0;
};

#if 0
TEST(GuidTest, GeneratesUniqueAndValidGuids)
{
    Guid g1 = Guid::generate();
    Guid g2 = Guid::generate();
    EXPECT_TRUE(g1.valid());
    EXPECT_TRUE(g2.valid());
    EXPECT_NE(g1, g2);
}

TEST(HandleTest, DefaultHandleIsInvalid)
{
    Handle<DummyResource> h;
    EXPECT_FALSE(h);
}

TEST(ResourcePoolTest, AddAndGetResource)
{
    ResourcePool<DummyResource> pool;
    auto h = pool.add(DummyResource{ 42 });
    EXPECT_TRUE(pool.valid(h));
    EXPECT_EQ(pool.get(h).value, 42);
}

TEST(ResourcePoolTest, VersionInvalidation)
{
    ResourcePool<DummyResource> pool;
    auto h = pool.add(DummyResource{ 5 });
    pool.remove(h);
    EXPECT_FALSE(pool.valid(h));
}

TEST(ResourcePoolTest, ReferenceCounting)
{
    ResourcePool<DummyResource> pool;
    auto h = pool.add(DummyResource{ 99 });
    pool.retain(h);
    pool.release(h);
    EXPECT_TRUE(pool.valid(h));
    pool.release(h);
    EXPECT_FALSE(pool.valid(h));
}

TEST(ResourcePoolTest, GuidBindingAndLookup)
{
    ResourcePool<DummyResource> pool;
    auto h = pool.add(DummyResource{ 13 });
    Guid guid = Guid::generate();
    pool.bind_guid(h, guid);
    EXPECT_EQ(pool.find_by_guid(guid), h);
    EXPECT_EQ(pool.guid_of(h), guid);
}

TEST(ResourceRegistryTest, AddAndRetrieveResource)
{
    ResourceRegistry registry;
    auto h = registry.add<DummyResource>(DummyResource{ 77 });
    EXPECT_TRUE(registry.valid(h));
    EXPECT_EQ(registry.get(h).value, 77);
}

TEST(ResourceRegistryTest, FindByGuid)
{
    ResourceRegistry registry;
    auto h = registry.add<DummyResource>(DummyResource{ 21 });
    Guid g = Guid::generate();
    registry.bind_guid(h, g);
    auto found = registry.find_by_guid<DummyResource>(g);
    EXPECT_TRUE(found);
    EXPECT_EQ(registry.get(found).value, 21);
}
#endif
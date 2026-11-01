#include <gtest/gtest.h>
#include "lru_cache.h"
#include "cache_test_invariants.h"

#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

using cache::LRUCache;

using namespace cache_test;

class LRUTest : public ::testing::Test
{
protected:
    std::unique_ptr<LRUCache<int, int>> cache;

    void init(std::size_t capacity)
    {
        cache = std::make_unique<LRUCache<int, int>>(capacity);
    }

    void TearDown() override
    {
        if (::testing::Test::HasFailure() && cache)
            cache->dump(std::cerr);
    }
};



std::vector<int> order_vec(const LRUCache<int, int>& c)
{
    return std::vector<int>(c.debug_order().begin(), c.debug_order().end());
}

std::size_t resident_count(const LRUCache<int, int>& c)
{
    return c.debug_order().size();
}



// --- unit ---

static void fill_seq(LRUCache<int, int>& c, int n)
{
    for (int i = 1; i <= n; i++) c.insert(i, i * 10);
}


TEST_F(LRUTest, InsertIntoEmpty)
{
    init(4);
    auto& c = *cache;

    auto ev = c.insert(1, 10);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(resident_count(c), 1u);
    EXPECT_EQ(order_vec(c), (std::vector<int>{1}));
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, InsertNewKeyNotFull)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);

    auto ev = c.insert(3, 30);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(resident_count(c), 3u);
    EXPECT_EQ(order_vec(c), (std::vector<int>{3, 2, 1}));
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, InsertExisting)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);  // 4 3 2 1

    auto ev = c.insert(2, 222);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(resident_count(c), 4u);
    EXPECT_EQ(order_vec(c), (std::vector<int>{2, 4, 3, 1}));
    EXPECT_EQ(*c.get(2), 222);
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, InsertEvictsLeastRecentlyUsed)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    auto ev = c.insert(99, 990);

    ASSERT_TRUE(ev.has_value());
    EXPECT_EQ(ev->first, 1);
    EXPECT_EQ(ev->second, 10);
    EXPECT_EQ(resident_count(c), 4u);
    EXPECT_EQ(order_vec(c), (std::vector<int>{99, 4, 3, 2}));
    EXPECT_FALSE(c.contains(1));
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, GetHitMovesToFront)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    auto v = c.get(2);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 20);
    EXPECT_EQ(order_vec(c), (std::vector<int>{2, 4, 3, 1}));
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, GetMissNoStateChange)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    const auto before = order_vec(c);

    auto v = c.get(999);

    EXPECT_FALSE(v.has_value());
    EXPECT_EQ(order_vec(c), before);
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, Contains)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    const auto before = order_vec(c);

    EXPECT_TRUE(c.contains(1));
    EXPECT_TRUE(c.contains(4));
    EXPECT_FALSE(c.contains(5));

    EXPECT_EQ(order_vec(c), before);
    check_lru_structure(c, "unit");
}


TEST_F(LRUTest, GetFirstDoesNotReorder)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    const auto before = order_vec(c);
    auto v = c.get(4);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(order_vec(c), before);
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, GetOnMiddleReodrers)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    c.get(2);

    EXPECT_EQ(order_vec(c), (std::vector<int>{2, 4, 3, 1}));
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, UpdateValueKeepsSize)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    auto ev = c.insert(1, 111);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(resident_count(c), 4u);
    EXPECT_EQ(order_vec(c), (std::vector<int>{1, 4, 3, 2}));
    EXPECT_EQ(*c.get(1), 111);
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, InsertExistingInFullCacheDoesNotEvict)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    auto ev = c.insert(3, 333);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(resident_count(c), 4u);
    EXPECT_EQ(order_vec(c), (std::vector<int>{3, 4, 2, 1}));
    EXPECT_EQ(*c.get(3), 333);
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, CapacityZeroBypass)
{
    init(0);
    auto& c = *cache;

    for (int i = 0; i < 500; i++)
    {
        auto ev = c.insert(i, i * 10);
        ASSERT_TRUE(ev.has_value());
        EXPECT_EQ(ev->first, i);
        EXPECT_EQ(ev->second, i * 10);

        EXPECT_FALSE(c.contains(i));
        EXPECT_FALSE(c.get(i).has_value());

        check_lru_structure(c, "i=" + std::to_string(i));
        if (::testing::Test::HasFailure()) return;
    }

    EXPECT_EQ(resident_count(c), 0u);
}

TEST_F(LRUTest, ExtractHitRemovesKey)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);  // 4 3 2 1

    auto v = c.extract_ptr(2);

    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 20);
    EXPECT_EQ(resident_count(c), 3u);
    EXPECT_EQ(order_vec(c), (std::vector<int>{4, 3, 1}));
    EXPECT_FALSE(c.contains(2));
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, ExtractMiss)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    const auto before = order_vec(c);

    auto v = c.extract_ptr(999);

    EXPECT_EQ(v, nullptr);
    EXPECT_EQ(order_vec(c), before);
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, ExtractMiddlePreservesOrder)
{
    init(5);
    auto& c = *cache;
    fill_seq(c, 5);

    c.extract_ptr(3);

    EXPECT_EQ(order_vec(c), (std::vector<int>{5, 4, 2, 1}));
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, ExtractAndReinsertGoesToFront)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    c.extract_ptr(2);
    c.insert(2, 222);

    EXPECT_EQ(order_vec(c), (std::vector<int>{2, 4, 3, 1}));
    EXPECT_EQ(*c.get(2), 222);
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, ExtractFromHead)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);  // 4 3 2 1

    auto v = c.extract_ptr(4);
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 40);
    EXPECT_EQ(order_vec(c), (std::vector<int>{3, 2, 1}));
    check_lru_structure(c, "unit");
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

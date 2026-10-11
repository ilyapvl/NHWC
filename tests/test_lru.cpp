#include <gtest/gtest.h>
#include "lru_cache.h"
#include "cache_test_common.h"

#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

using cache::LRUCache;

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



void check_lru_structure(const LRUCache<int, int>& c, const std::string& what)
{
    SCOPED_TRACE(what);

    const auto& order = c.debug_order();
    const auto& pos   = c.debug_positions();
    const auto& vals  = c.debug_values();

    EXPECT_EQ(order.size(), pos.size())   << "order vs positions size";
    EXPECT_EQ(order.size(), vals.size())  << "order vs values size";
    EXPECT_LE(order.size(), c.capacity()) << "size exceeds capacity";


    // no duplicates
    std::unordered_set<int> seen;
  
    for (int k : order)
    {
        EXPECT_TRUE(seen.insert(k).second) << "duplicate in order: " << k;
    }

    // key without value/position
    for (auto it = order.begin(); it != order.end(); it++)
    {
        auto pit = pos.find(*it);
        ASSERT_NE(pit, pos.end()) << "resident key " << *it << " not in positions";
        EXPECT_EQ(pit->second, it) << "iterator mismatch for " << *it;

        auto vit = vals.find(*it);
        ASSERT_NE(vit, vals.end()) << "resident key " << *it << " has no value";
        EXPECT_TRUE(vit->second) << "null value for " << *it;
    }

    // all values have positions
    for (const auto& [k, _] : pos)
    {
        EXPECT_NE(vals.find(k), vals.end()) << "position without value: " << k;
        EXPECT_TRUE(seen.count(k)) << "position without order entry: " << k;
    }
    for (const auto& [k, _] : vals)
    {
        EXPECT_NE(pos.find(k), pos.end()) << "value without position: " << k;
        EXPECT_TRUE(seen.count(k)) << "value without order entry: " << k;
    }
}

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

TEST_F(LRUTest, ExtractHitRemovesKey)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    auto v = c.extract(2);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 20);
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

    auto v = c.extract(999);

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

TEST_F(LRUTest, ExtractMiddlePreservesOrder)
{
    init(5);
    auto& c = *cache;
    fill_seq(c, 5);

    c.extract(3);

    EXPECT_EQ(order_vec(c), (std::vector<int>{5, 4, 2, 1}));
    check_lru_structure(c, "unit");
}

TEST_F(LRUTest, ExtractAndReinsertGoesToFront)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    c.extract(2);
    c.insert(2, 222);

    EXPECT_EQ(order_vec(c), (std::vector<int>{2, 4, 3, 1}));
    EXPECT_EQ(*c.get(2), 222);
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
        EXPECT_FALSE(c.extract(i).has_value());

        check_lru_structure(c, "i=" + std::to_string(i));
        if (::testing::Test::HasFailure()) return;
    }

    EXPECT_EQ(resident_count(c), 0u);
}



// --- long sequence ---

using LRUInv = CacheInvTest<LRUCache<int, int>>;

TEST_P(LRUInv, InvTest)
{
    auto& c = *cache;

    const std::size_t CAP       = GetParam().cap;
    const int         KEY_RANGE = GetParam().key_range;
    const int         OPS       = GetParam().ops;

    std::mt19937 rng(GetParam().seed);
    std::uniform_int_distribution<int> kd(0, KEY_RANGE - 1);
    std::uniform_int_distribution<int> od(0, 99);

    std::size_t cnt_ins_new      = 0;
    std::size_t cnt_ins_existing = 0;
    std::size_t cnt_ins_evict    = 0;
    std::size_t cnt_get_hit      = 0;
    std::size_t cnt_get_miss     = 0;
    std::size_t cnt_ext_hit      = 0;
    std::size_t cnt_ext_miss     = 0;
    std::size_t cnt_contains     = 0;

    for (int i = 0; i < OPS; i++)
    {
        const int k = kd(rng);
        const int o = od(rng);

        const std::string ctx = "step=" + std::to_string(i) +
                                " op=" + std::to_string(o) +
                                " key=" + std::to_string(k);

        if (o < 50)
        {
            const bool        existed     = c.contains(k);
            const std::size_t before_size = resident_count(c);
            const bool        full        = (before_size == CAP);
            const int         lru_before  = c.debug_order().empty()
                                            ? -1
                                            : c.debug_order().back();

            auto ev = c.insert(k, i);

            if (existed)
            {
                cnt_ins_existing++;
                EXPECT_FALSE(ev.has_value()) << ctx;
                EXPECT_EQ(resident_count(c), before_size) << ctx;
            }

            else if (full)
            {
                cnt_ins_evict++;
                ASSERT_TRUE(ev.has_value()) << ctx;
                EXPECT_EQ(ev->first, lru_before) << ctx;
                EXPECT_EQ(resident_count(c), CAP) << ctx;
                EXPECT_FALSE(c.contains(ev->first)) << ctx;
            }

            else
            {
                cnt_ins_new++;
                EXPECT_FALSE(ev.has_value()) << ctx;
                EXPECT_EQ(resident_count(c), before_size + 1) << ctx;
            }

            ASSERT_FALSE(c.debug_order().empty()) << ctx;
            EXPECT_EQ(c.debug_order().front(), k) << ctx;
        }

        else if (o < 80)
        {
            const auto before = order_vec(c);

            auto v = c.get(k);

            if (v.has_value())
            {
                cnt_get_hit++;
                EXPECT_EQ(resident_count(c), before.size()) << ctx;
                ASSERT_FALSE(c.debug_order().empty()) << ctx;
                EXPECT_EQ(c.debug_order().front(), k) << ctx;
            }

            else
            {
                cnt_get_miss++;
                EXPECT_EQ(order_vec(c), before) << ctx;
            }
        }

        else if (o < 95)
        {
            const auto before = order_vec(c);

            auto v = c.extract(k);

            if (v.has_value())
            {
                cnt_ext_hit++;
                EXPECT_FALSE(c.contains(k)) << ctx;
                EXPECT_FALSE(c.get(k).has_value()) << ctx;
                EXPECT_EQ(resident_count(c), before.size() - 1) << ctx;
            }

            else
            {
                cnt_ext_miss++;
                EXPECT_EQ(order_vec(c), before) << ctx;
            }
        }
        else
        {
            const auto before = order_vec(c);

            cnt_contains++;
            c.contains(k);

            EXPECT_EQ(order_vec(c), before) << ctx;
        }

        check_lru_structure(c, ctx);
        if (::testing::Test::HasFailure()) return;
    }

    std::cerr << "[cap=" << CAP << " key_range=" << KEY_RANGE
            << " seed=" << GetParam().seed << "] "
            << "ins_new=" << cnt_ins_new
            << " ins_existing=" << cnt_ins_existing
            << " ins_evict=" << cnt_ins_evict
            << " get_hit=" << cnt_get_hit
            << " get_miss=" << cnt_get_miss
            << " ext_hit=" << cnt_ext_hit
            << " ext_miss=" << cnt_ext_miss
            << " contains=" << cnt_contains << '\n';

    check_lru_structure(c, "final");
}

INSTANTIATE_TEST_SUITE_P(LruSizes, LRUInv, StandardInvParams());

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

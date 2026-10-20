#include <gtest/gtest.h>
#include "lfu_cache.h"
#include "cache_test_invariants.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

using cache::LFUCache;

using namespace cache_test;

class LFUTest : public ::testing::Test
{
protected:
    std::unique_ptr<LFUCache<int, int>> cache;

    void init(std::size_t capacity)
    {
        cache = std::make_unique<LFUCache<int, int>>(capacity, capacity);
    }

    void TearDown() override
    {
        if (::testing::Test::HasFailure() && cache)
            cache->dump(std::cerr);
    }
};


std::size_t resident_count(const LFUCache<int, int>& c)
{
    return c.debug_key_to_pair().size();
}


static void fill_seq(LFUCache<int, int>& c, int n)
{
    for (int i = 1; i <= n; i++) c.insert(i, i * 10);
}


// --- unit ---


TEST_F(LFUTest, InsertIntoEmpty)
{
    init(4);
    auto& c = *cache;

    auto ev = c.insert(1, 10);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(resident_count(c), 1u);
    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 1);
    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, InsertNewKeyNotFull)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);

    EXPECT_EQ(resident_count(c), 3u);
    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 1);
    EXPECT_EQ(c.debug_key_to_pair().at(2).first, 1);
    EXPECT_EQ(c.debug_key_to_pair().at(3).first, 1);

    // reverse insertion order
    ASSERT_TRUE(c.debug_freq_to_keys().count(1));
    EXPECT_EQ(c.debug_freq_to_keys().at(1).front(), 3);
    EXPECT_EQ(c.debug_freq_to_keys().at(1).back(), 1);

    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, InsertExisting)
{
    init(4);
    auto& c = *cache;
    c.insert(1, 10);
    c.insert(2, 20);

    auto ev = c.insert(1, 111);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(resident_count(c), 2u);
    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 2);

    ASSERT_TRUE(c.debug_freq_to_keys().count(2));
    EXPECT_EQ(c.debug_freq_to_keys().at(2).front(), 1);
    EXPECT_EQ(*c.get(1), 111);

    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, InsertEvictsLowestFreq)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);  // 1: [4 3 2 1]

    auto ev = c.insert(99, 990);

    ASSERT_TRUE(ev.has_value());
    EXPECT_EQ(ev->first, 1);
    EXPECT_EQ(ev->second, 10);
    EXPECT_EQ(resident_count(c), 4u);
    EXPECT_FALSE(c.contains(1));

    // new key at front of freq 1
    ASSERT_TRUE(c.debug_freq_to_keys().count(1));
    EXPECT_EQ(c.debug_freq_to_keys().at(1).front(), 99);
    EXPECT_EQ(c.debug_freq_to_keys().at(1).back(), 2);

    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, GetHitIncrements)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    auto v = c.get(2);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 20);
    EXPECT_EQ(c.debug_key_to_pair().at(2).first, 2);

    ASSERT_TRUE(c.debug_freq_to_keys().count(2));
    EXPECT_EQ(c.debug_freq_to_keys().at(2).front(), 2);

    // freq 1 -> freq 2
    ASSERT_TRUE(c.debug_freq_to_keys().count(1));
    EXPECT_EQ(std::find(c.debug_freq_to_keys().at(1).begin(), c.debug_freq_to_keys().at(1).end(), 2), c.debug_freq_to_keys().at(1).end());

    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, GetMissNoStateChange)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    auto v = c.get(999);

    EXPECT_FALSE(v.has_value());
    EXPECT_EQ(resident_count(c), 4u);
    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 1);
    EXPECT_EQ(c.debug_key_to_pair().at(4).first, 1);

    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, Contains)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    EXPECT_TRUE(c.contains(1));
    EXPECT_TRUE(c.contains(4));
    EXPECT_FALSE(c.contains(5));

    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 1);
    EXPECT_EQ(c.debug_key_to_pair().at(4).first, 1);

    check_lfu_structure(c, "unit");
}




TEST_F(LFUTest, InsertAndGetIncrementsFreq)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(1, 20);
    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 2);

    auto v = c.get(1);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 20);
    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 3);

    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, InsertExistingInFullCacheDoesNotEvict)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    auto ev = c.insert(3, 333);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(resident_count(c), 4u);
    EXPECT_EQ(c.debug_key_to_pair().at(3).first, 2);

    EXPECT_EQ(*c.get(3), 333);



    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, PromoteToHigherfreq)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    // 1: [2 1]

    ASSERT_EQ(c.debug_key_to_pair().at(1).first, 1);
    ASSERT_EQ(c.debug_key_to_pair().at(2).first, 1);

    c.get(1);

    // 1: [2], 2: [1]

    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 2);
    EXPECT_EQ(c.debug_key_to_pair().at(2).first, 1);

    ASSERT_TRUE(c.debug_freq_to_keys().count(2));
    EXPECT_EQ(c.debug_freq_to_keys().at(2).front(), 1);
    ASSERT_TRUE(c.debug_freq_to_keys().count(1));
    EXPECT_EQ(c.debug_freq_to_keys().at(1).front(), 2);

    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, EvictionPicksSmallestFreq)
{
    init(3);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);

    for (int i = 0; i < 10; i++) c.get(1); // 11
    for (int i = 0; i < 5; i++)  c.get(2); // 6
    c.get(3); // 2

    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 11);
    EXPECT_EQ(c.debug_key_to_pair().at(2).first, 6);
    EXPECT_EQ(c.debug_key_to_pair().at(3).first, 2);

    auto ev = c.insert(4, 40);

    ASSERT_TRUE(ev.has_value());

    EXPECT_EQ(ev->first, 3);

    EXPECT_FALSE(c.contains(3));
    EXPECT_TRUE(c.contains(1));
    EXPECT_TRUE(c.contains(2));
    EXPECT_TRUE(c.contains(4));




    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, EvictionPicksLRUWithinBucket)
{
    init(3);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    // 1: [3, 2, 1]

    c.get(1);
    // [3, 2], 2: [1]

    auto ev = c.insert(4, 40);
    ASSERT_TRUE(ev.has_value());

    EXPECT_EQ(ev->first, 2);  // back of 1 

    ASSERT_TRUE(c.debug_freq_to_keys().count(1));
    EXPECT_EQ(c.debug_freq_to_keys().at(1).front(), 4);
    EXPECT_EQ(c.debug_freq_to_keys().at(1).back(), 3);

    check_lfu_structure(c, "unit");
}




TEST_F(LFUTest, ExtractHitPreservesFrequencyInGhost)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    for (int i = 0; i < 5; i++) c.get(1);  // freq = 6

    ASSERT_EQ(c.debug_key_to_pair().at(1).first, 6);

    auto v = c.extract_ptr(1);

    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 10);
    EXPECT_FALSE(c.contains(1));
    EXPECT_TRUE(c.debug_in_ghost(1));
    EXPECT_EQ(c.debug_ghost_freq(1), 6);
    EXPECT_EQ(resident_count(c), 0u);
    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, ExtractMiss)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    auto v = c.extract_ptr(999);

    EXPECT_EQ(v, nullptr);
    EXPECT_EQ(resident_count(c), 4u);
    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, ReinsertAfterExtractRestoresFrequency)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    for (int i = 0; i < 5; i++) c.get(1);  // freq = 6

    c.extract_ptr(1);
    ASSERT_TRUE(c.debug_in_ghost(1));

    c.insert(1, 11);

    EXPECT_TRUE(c.contains(1));
    EXPECT_FALSE(c.debug_in_ghost(1));
    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 6);
    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, GetOnGhostKeyReturnsMiss)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);

    for (int i = 0; i < 5; i++) c.get(1);
    c.extract_ptr(1);

    ASSERT_TRUE(c.debug_in_ghost(1));
    ASSERT_EQ(c.debug_ghost_freq(1), 6);

    auto v = c.get(1);

    EXPECT_FALSE(v.has_value());
    EXPECT_TRUE(c.debug_in_ghost(1));
    EXPECT_EQ(c.debug_ghost_freq(1), 6);

    c.insert(1, 11);
    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 6);





    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, ContainsOnGhostKeyReturnsFalse)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.extract_ptr(1);
    ASSERT_TRUE(c.debug_in_ghost(1));

    EXPECT_FALSE(c.contains(1));
    check_lfu_structure(c, "unit");
}



TEST_F(LFUTest, UpdateValueKeepsSize)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);  // 1: [4 3 2 1]

    const auto before_size = resident_count(c);

    auto ev = c.insert(1, 111);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(resident_count(c), before_size);
    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 2);
    EXPECT_EQ(*c.debug_values().at(1), 111);
    check_lfu_structure(c, "unit");
}


int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

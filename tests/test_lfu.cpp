#include <gtest/gtest.h>
#include "lfu_cache.h"
#include "cache_test_common.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

using cache::LFUCache;

class LFUTest : public ::testing::Test
{
protected:
    std::unique_ptr<LFUCache<int, int>> cache;

    void init(std::size_t capacity)
    {
        cache = std::make_unique<LFUCache<int, int>>(capacity);
    }

    void TearDown() override
    {
        if (::testing::Test::HasFailure() && cache)
            cache->dump(std::cerr);
    }
};


void check_lfu_structure(const LFUCache<int, int>& c, const std::string& what)
{
    SCOPED_TRACE(what);

    const auto& ftk  = c.debug_freq_to_keys();
    const auto& ktp  = c.debug_key_to_pair();
    const auto& vals = c.debug_values();

    // sizes
    EXPECT_EQ(ktp.size(), vals.size())         << "key_to_pair vs values size";
    EXPECT_LE(ktp.size(), c.capacity())        << "size exceeds capacity";
    EXPECT_LE(c.debug_ghost_size(), c.debug_ghost_capacity()) << "ghost exceeds capacity";

    // all freqs > 0, keys within a frequency are unique
    std::size_t total_in_buckets = 0;
    for (const auto& [freq, lst] : ftk)
    {
        EXPECT_GT(freq, 0)          << "non-positive freq: " << freq;
        EXPECT_FALSE(lst.empty())   << "empty bucket at freq " << freq;

        std::unordered_set<int> within;
        for (int k : lst)
        {
            EXPECT_TRUE(within.insert(k).second)
                << "duplicate key " << k << " in bucket " << freq;
        }
        total_in_buckets += lst.size();
    }

    // sum of all frequencies' keys == total size
    EXPECT_EQ(total_in_buckets, ktp.size()) << "buckets hold a different count than the map";

    // ach key in a bucket is in key_to_pair with matching freq and iterator
    for (const auto& [freq, lst] : ftk)
    {
        for (auto it = lst.begin(); it != lst.end(); it++)
        {
            auto kit = ktp.find(*it);
            ASSERT_NE(kit, ktp.end())
                << "key " << *it << " in bucket " << freq << " but not in key_to_pair";
            EXPECT_EQ(kit->second.first, freq)
                << "freq mismatch for " << *it;
            EXPECT_EQ(kit->second.second, it)
                << "iterator mismatch for " << *it;
        }
    }

    // each entry of key_to_pair is present in the bucket it claims
    for (const auto& [k, pair] : ktp)
    {
        const int f = pair.first;
        auto bit = ftk.find(f);
        ASSERT_NE(bit, ftk.end()) << "bucket " << f << " missing for key " << k;

        const auto& bucket = bit->second;
        EXPECT_NE(std::find(bucket.begin(), bucket.end(), k), bucket.end()) << "key " << k << " not in its claimed bucket " << f;

        ASSERT_NE(vals.find(k), vals.end()) << "resident key " << k << " has no value";
        EXPECT_TRUE(vals.at(k)) << "null value for " << k;
    }

    // no extra keys in values
    for (const auto& [k, _] : vals)
    {
        EXPECT_NE(ktp.find(k), ktp.end()) << "value without key_to_pair entry: " << k;
    }
}


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

TEST_F(LFUTest, ExtractHitRemovesKey)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    auto v = c.extract(2);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 20);
    EXPECT_FALSE(c.contains(2));
    EXPECT_EQ(resident_count(c), 3u);
    EXPECT_FALSE(c.debug_key_to_pair().count(2));




    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, ExtractMiss)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    auto v = c.extract(999);

    EXPECT_FALSE(v.has_value());
    EXPECT_EQ(resident_count(c), 4u);




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

TEST_F(LFUTest, ExtractReinsertRestoresFrequency)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    for (int i = 0; i < 5; i++) c.get(1); // freq 6

    ASSERT_EQ(c.debug_key_to_pair().at(1).first, 6);

    c.extract(1);

    EXPECT_FALSE(c.contains(1));
    EXPECT_TRUE(c.debug_in_ghost(1));
    EXPECT_EQ(c.debug_ghost_freq(1), 6);

    c.insert(1, 11);

    EXPECT_TRUE(c.contains(1));
    EXPECT_FALSE(c.debug_in_ghost(1));

    EXPECT_EQ(c.debug_key_to_pair().at(1).first, 6);  // restored

    check_lfu_structure(c, "unit");
}

TEST_F(LFUTest, GetDoesNotReturnSystemGhost)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    for (int i = 0; i < 5; i++) c.get(1); // freq 6
    c.extract(1);

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


// --- long sequence ---


using LFUInv = CacheInvTest<LFUCache<int, int>>;

TEST_P(LFUInv, InvTest)
{
    const std::size_t CAP       = GetParam().cap;
    const int         KEY_RANGE = GetParam().key_range;
    const int         OPS       = GetParam().ops;

    auto& c = *cache;

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
            const std::size_t before_size = c.debug_key_to_pair().size();
            const bool        full        = (before_size == CAP);

            const int old_freq = existed ? c.debug_key_to_pair().at(k).first : 0;

            // expected back of the smallest-frequency bucket
            int expected_victim = -1;
            if (!existed && full)
            {
                auto& mfk = c.debug_freq_to_keys();
                ASSERT_FALSE(mfk.empty()) << ctx;
                ASSERT_FALSE(mfk.begin()->second.empty()) << ctx;
                expected_victim = mfk.begin()->second.back();
            }

            // expected frequency for a new insert == 1 or restored from ghost
            const bool was_in_ghost = !existed && c.debug_in_ghost(k);
            const int expected_new_freq = was_in_ghost ? c.debug_ghost_freq(k) : 1;

            auto ev = c.insert(k, i);

            if (existed)
            {
                cnt_ins_existing++;
                EXPECT_FALSE(ev.has_value()) << ctx;
                EXPECT_EQ(c.debug_key_to_pair().size(), before_size) << ctx;

                const int new_freq = c.debug_key_to_pair().at(k).first;
                EXPECT_EQ(new_freq, old_freq + 1) << ctx;

                auto bit = c.debug_freq_to_keys().find(new_freq);
                ASSERT_NE(bit, c.debug_freq_to_keys().end()) << ctx;
                ASSERT_FALSE(bit->second.empty()) << ctx;
                EXPECT_EQ(bit->second.front(), k) << ctx;
            }

            else if (full)
            {
                cnt_ins_evict++;
                ASSERT_TRUE(ev.has_value()) << ctx;
                EXPECT_EQ(ev->first, expected_victim) << ctx;
                EXPECT_EQ(c.debug_key_to_pair().size(), CAP) << ctx;
                EXPECT_FALSE(c.contains(ev->first)) << ctx;

                const int new_freq = c.debug_key_to_pair().at(k).first;
                EXPECT_EQ(new_freq, expected_new_freq) << ctx;

                auto bit = c.debug_freq_to_keys().find(new_freq);
                ASSERT_NE(bit, c.debug_freq_to_keys().end()) << ctx;
                ASSERT_FALSE(bit->second.empty()) << ctx;
                EXPECT_EQ(bit->second.front(), k) << ctx;
            }


            else
            {
                cnt_ins_new++;
                EXPECT_FALSE(ev.has_value()) << ctx;
                EXPECT_EQ(c.debug_key_to_pair().size(), before_size + 1) << ctx;

                const int new_freq = c.debug_key_to_pair().at(k).first;
                EXPECT_EQ(new_freq, expected_new_freq) << ctx;

                auto bit = c.debug_freq_to_keys().find(new_freq);
                ASSERT_NE(bit, c.debug_freq_to_keys().end()) << ctx;
                ASSERT_FALSE(bit->second.empty()) << ctx;
                EXPECT_EQ(bit->second.front(), k) << ctx;
            }
        }


        else if (o < 80)
        {
            const int old_freq = c.contains(k) ? c.debug_key_to_pair().at(k).first : 0;
            const std::size_t before_size = c.debug_key_to_pair().size();

            auto v = c.get(k);

            if (v.has_value())
            {
                cnt_get_hit++;
                EXPECT_EQ(c.debug_key_to_pair().size(), before_size) << ctx;

                const int new_freq = c.debug_key_to_pair().at(k).first;
                EXPECT_EQ(new_freq, old_freq + 1) << ctx;

                auto bit = c.debug_freq_to_keys().find(new_freq);
                ASSERT_NE(bit, c.debug_freq_to_keys().end()) << ctx;
                ASSERT_FALSE(bit->second.empty()) << ctx;
                EXPECT_EQ(bit->second.front(), k) << ctx;
            }

            else
            {
                cnt_get_miss++;
                EXPECT_EQ(c.debug_key_to_pair().size(), before_size) << ctx;
            }
        }
        else if (o < 95)
        {
            const std::size_t before_size = c.debug_key_to_pair().size();

            auto v = c.extract(k);

            if (v.has_value())
            {
                cnt_ext_hit++;
                EXPECT_FALSE(c.contains(k)) << ctx;
                EXPECT_FALSE(c.get(k).has_value())   << ctx;
                EXPECT_EQ(c.debug_key_to_pair().size(), before_size - 1) << ctx;
            }
            else
            {
                cnt_ext_miss++;
                EXPECT_EQ(c.debug_key_to_pair().size(), before_size) << ctx;
            }
        }

        else
        {
            const std::size_t before_size = c.debug_key_to_pair().size();
            cnt_contains++;
            c.contains(k);
            EXPECT_EQ(c.debug_key_to_pair().size(), before_size) << ctx;
        }

        check_lfu_structure(c, ctx);
        if (::testing::Test::HasFailure()) return;
    }

    std::cerr << "[executed operations] ins_new=" << cnt_ins_new
              << " ins_existing=" << cnt_ins_existing
              << " ins_evict=" << cnt_ins_evict
              << " get_hit=" << cnt_get_hit
              << " get_miss=" << cnt_get_miss
              << " ext_hit=" << cnt_ext_hit
              << " ext_miss=" << cnt_ext_miss
              << " contains=" << cnt_contains << '\n';

    check_lfu_structure(c, "final");
}

INSTANTIATE_TEST_SUITE_P(LfuSizes, LFUInv, StandardInvParams());

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

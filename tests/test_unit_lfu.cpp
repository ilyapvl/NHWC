#include <gtest/gtest.h>
#include "lfu_cache.h"

#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>


class DumpOnFailure
{
public:
    explicit DumpOnFailure(const LFUCache<int, int>& cache) : m_cache(cache) {}

    ~DumpOnFailure()
    {
        if (!::testing::Test::HasFailure() || s_already_reported) return;
        s_already_reported = true;
        m_cache.dump(std::cerr);
    }

private:
    const LFUCache<int, int>& m_cache;
    static inline bool s_already_reported = false;
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


TEST(LFU, InvTest)
{
    constexpr std::size_t CAP       = 1024;
    constexpr int         KEY_RANGE = 4096;
    constexpr int         OPS       = 100000;

    LFUCache<int, int> c(CAP);

    std::mt19937 rng(1234);
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

    for (int i = 0; i < OPS; ++i)
    {
        DumpOnFailure guard(c);

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

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

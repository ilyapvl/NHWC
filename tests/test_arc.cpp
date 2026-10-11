#include <gtest/gtest.h>
#include "arc_cache.h"
#include "cache_test_common.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>



using cache::ARCCache;


constexpr int ARC_UNKNOWN = -1;
constexpr int ARC_NONE    = 0;
constexpr int ARC_T1      = 1;
constexpr int ARC_T2      = 2;
constexpr int ARC_B1      = 3;
constexpr int ARC_B2      = 4;

const char* tag_name(int tag)
{
    switch (tag)
    {
        case ARC_UNKNOWN: return "UNKNOWN";
        case ARC_NONE:    return "NONE";
        case ARC_T1:      return "T1";
        case ARC_T2:      return "T2";
        case ARC_B1:      return "B1";
        case ARC_B2:      return "B2";
    }
    return "?";
}


class ARCTest : public ::testing::Test
{
protected:
    std::unique_ptr<ARCCache<int, int>> cache;

    void init(std::size_t capacity)
    {
        cache = std::make_unique<ARCCache<int, int>>(capacity);
    }

    void TearDown() override
    {
        if (::testing::Test::HasFailure() && cache)
            cache->dump(std::cerr);
    }
};


void check_arc_structure(const ARCCache<int, int>& c, const std::string& what)
{
    SCOPED_TRACE(what);

    const auto& t1    = c.debug_t1();
    const auto& t2    = c.debug_t2();
    const auto& b1    = c.debug_b1();
    const auto& b2    = c.debug_b2();
    const auto& infos = c.debug_element_infos();
    const auto& vals  = c.debug_values();

    const std::size_t cap = c.capacity();

    // arc lists sizes
    EXPECT_LE(t1.size() + t2.size(), cap) << "|T1| + |T2| > capacity";
    EXPECT_LE(t1.size() + b1.size(), cap) << "|T1| + |B1| > capacity";
    EXPECT_LE(t1.size() + t2.size() + b1.size() + b2.size(), 2 * cap) << "|T1| + |T2| + |B1| + |B2| > 2 * capacity";
    EXPECT_LE(c.debug_target_t1_size(), cap) << "target_t1_size > capacity";

    // values mirror resident set
    EXPECT_EQ(vals.size(), t1.size() + t2.size()) << "values size != |T1| + |T2|";

    // each list has no duplicates and iterators are valid
    std::unordered_set<int> all_seen;

    auto walk = [&](const std::list<int>& lst, int expected_tag, bool resident)
    {
        std::unordered_set<int> local;
        for (int k : lst)
        {
            EXPECT_TRUE(local.insert(k).second) << "duplicate in list " << tag_name(expected_tag) << ": " << k;
            EXPECT_TRUE(all_seen.insert(k).second) << "key " << k << " appears in more than one list";

            const int tag = c.debug_list_tag(k);
            EXPECT_EQ(tag, expected_tag) << "list tag mismatch for " << k
                << " (expected " << tag_name(expected_tag)
                << ", got " << tag_name(tag) << ")";

            EXPECT_TRUE(c.debug_iterator_valid(k)) << "iterator invalid for " << k;

            EXPECT_EQ(c.contains(k), resident) << "residency mismatch for " << k;

            if (resident)
            {
                auto vit = vals.find(k);
                ASSERT_NE(vit, vals.end()) << "resident key " << k << " has no value";
                EXPECT_TRUE(vit->second)  << "null value for resident " << k;
            }

            else
            {
                EXPECT_EQ(vals.find(k), vals.end()) << "ghost key " << k << " has a value";
            }
        }
    };

    walk(t1, ARC_T1, true);
    walk(t2, ARC_T2, true);
    walk(b1, ARC_B1, false);
    walk(b2, ARC_B2, false);

    std::size_t num_infos = 0;
    for (const auto& [k, info] : infos)
    {
        num_infos++;

        EXPECT_TRUE(all_seen.count(k) > 0) << "key " << k << " in element_infos but not in any list";

        const int  tag             = c.debug_list_tag(k);
        const bool is_resident_tag = (tag == ARC_T1 || tag == ARC_T2);

        EXPECT_EQ(info.resident, is_resident_tag) << "resident flag disagrees with list tag for key " << k;

        if (info.resident)
        {
            EXPECT_NE(vals.find(k), vals.end()) << "resident key " << k << " has no value";
        }

        else
        {
            EXPECT_EQ(vals.find(k), vals.end()) << "ghost key " << k << " has a value";
        }
    }

    EXPECT_EQ(num_infos, all_seen.size())<< "element_infos size does not match total keys in lists";

    // values contain elements keys only in T1 / T2
    for (const auto& [k, _] : vals)
    {
        const int tag = c.debug_list_tag(k);
        EXPECT_TRUE(tag == ARC_T1 || tag == ARC_T2)
            << "value for non-resident key " << k
            << " (tag " << tag_name(tag) << ")";
    }
}


// --- unit ---


TEST_F(ARCTest, InsertIntoEmptyInT1)
{
    init(4);
    auto& c = *cache;

    auto ev = c.insert(1, 10);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(c.debug_list_tag(1), ARC_T1);
    EXPECT_EQ(c.debug_t1().front(), 1);
    EXPECT_EQ(c.debug_target_t1_size(), 0u);

    check_arc_structure(c, "unit");
}


TEST_F(ARCTest, InsertExistinToT2)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    ASSERT_EQ(c.debug_list_tag(1), ARC_T1);

    auto ev = c.insert(1, 11);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(c.debug_t1().size(), 0u);
    EXPECT_EQ(c.debug_list_tag(1), ARC_T2);
    EXPECT_EQ(c.debug_t2().front(), 1);


    check_arc_structure(c, "unit");
}

TEST_F(ARCTest, InsertExistingInT2StaysInT2)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.get(1);
    ASSERT_EQ(c.debug_list_tag(1), ARC_T2);

    auto ev = c.insert(1, 11);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(c.debug_list_tag(1), ARC_T2);
    EXPECT_EQ(c.debug_t2().front(), 1);
    check_arc_structure(c, "unit");
}

TEST_F(ARCTest, T1HitMovesToT2)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    ASSERT_EQ(c.debug_list_tag(1), ARC_T1);

    auto v = c.get(1);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 10);
    EXPECT_EQ(c.debug_list_tag(1), ARC_T2);
    EXPECT_EQ(c.debug_t1().size(), 1u);
    EXPECT_EQ(c.debug_t1().front(), 2);
    EXPECT_EQ(c.debug_t2().front(), 1);
    check_arc_structure(c, "unit");
}

TEST_F(ARCTest, T2HitMovesToFront)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.get(1);
    c.get(2);
    

    ASSERT_EQ(c.debug_t2().front(), 2);
    ASSERT_EQ(c.debug_t2().back(), 1);

    auto v = c.get(1);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 10);
    EXPECT_EQ(c.debug_t2().front(), 1);
    EXPECT_EQ(c.debug_t2().back(), 2);

    check_arc_structure(c, "unit");
}



TEST_F(ARCTest, ExtractT1SavesToGhostAsT1)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    ASSERT_EQ(c.debug_list_tag(1), ARC_T1);

    auto v = c.extract(1);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 10);
    EXPECT_FALSE(c.contains(1));
    EXPECT_TRUE(c.debug_in_ghost(1));
    EXPECT_FALSE(c.debug_ghost_is_t2(1));
    EXPECT_EQ(c.debug_t1().size(), 0u);

    check_arc_structure(c, "unit");
}

TEST_F(ARCTest, ExtractT2SavesToGhostAsT2)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.get(1);
    ASSERT_EQ(c.debug_list_tag(1), ARC_T2);

    auto v = c.extract(1);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 10);
    EXPECT_FALSE(c.contains(1));
    EXPECT_TRUE(c.debug_in_ghost(1));
    EXPECT_TRUE(c.debug_ghost_is_t2(1));
    EXPECT_EQ(c.debug_t2().size(), 0u);
    
    check_arc_structure(c, "unit");
}

TEST_F(ARCTest, Contains)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);

    EXPECT_TRUE(c.contains(1));
    EXPECT_FALSE(c.contains(999));

    EXPECT_EQ(c.debug_t1().size(), 2u);
    EXPECT_EQ(c.debug_t2().size(), 0u);
    EXPECT_EQ(c.debug_b1().size(), 0u);
    EXPECT_EQ(c.debug_b2().size(), 0u);


    check_arc_structure(c, "unit");
}



TEST_F(ARCTest, ReinsertAfterExtractT1ReturnsToT1)
{
    init(2);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);

    ASSERT_EQ(c.debug_list_tag(1), ARC_T1);

    c.extract(1);

    ASSERT_TRUE(c.debug_in_ghost(1));
    ASSERT_FALSE(c.debug_ghost_is_t2(1));

    c.insert(1, 11);

    EXPECT_EQ(c.debug_list_tag(1), ARC_T1);
    EXPECT_EQ(c.debug_t1().front(), 1);


    check_arc_structure(c, "unit");
}

TEST_F(ARCTest, ReinsertAfterExtractT2ReturnsToT2)
{
    init(2);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.get(1);
    ASSERT_EQ(c.debug_list_tag(1), ARC_T2);

    c.extract(1);
    ASSERT_TRUE(c.debug_in_ghost(1));
    ASSERT_TRUE(c.debug_ghost_is_t2(1));

    c.insert(1, 11);

    EXPECT_EQ(c.debug_list_tag(1), ARC_T2);
    EXPECT_EQ(c.debug_t2().front(), 1);
    check_arc_structure(c, "unit");
}



TEST_F(ARCTest, B1HitPromotesToT2AndIncreasesP)
{
    init(3);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);

    c.get(1);

    // T1.back() = 2 to B1
    auto ev4 = c.insert(4, 40);
    ASSERT_TRUE(ev4.has_value());
    EXPECT_EQ(ev4->first, 2);
    ASSERT_EQ(c.debug_list_tag(2), ARC_B1);
    ASSERT_EQ(c.debug_target_t1_size(), 0u);

    // p increases by 1, key moves to T2
    // T1.back() evicted to B1
    auto ev2 = c.insert(2, 22);

    ASSERT_TRUE(ev2.has_value());
    EXPECT_EQ(ev2->first, 3);   

    EXPECT_EQ(c.debug_target_t1_size(), 1u) << "p must increase on B1 hit";
    EXPECT_EQ(c.debug_list_tag(2), ARC_T2);
    EXPECT_EQ(c.debug_t2().front(), 2);


    check_arc_structure(c, "unit");
}

TEST_F(ARCTest, B2HitPromotesToT2AndDecreasesP)
{
    init(3);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);

    c.get(3);
    c.get(2);
    c.get(1); // T2 = [1 2 3]

    // replace evicts T2.back() = 3 to B2
    auto ev4 = c.insert(4, 40);
    ASSERT_TRUE(ev4.has_value());
    EXPECT_EQ(ev4->first, 3);
    ASSERT_EQ(c.debug_list_tag(3), ARC_B2);

    // replace evicts T1.back() = 4 to B1
    auto ev5 = c.insert(5, 50);
    ASSERT_TRUE(ev5.has_value());
    EXPECT_EQ(ev5->first, 4);
    ASSERT_EQ(c.debug_list_tag(4), ARC_B1);

    // B1 hit so p = 1
    auto ev4b = c.insert(4, 41);
    ASSERT_TRUE(ev4b.has_value());
    ASSERT_EQ(c.debug_target_t1_size(), 1u);

    // B2 hit so p = 0, replace evicts T1.back() to B1
    auto ev3 = c.insert(3, 33);

    ASSERT_TRUE(ev3.has_value());
    EXPECT_EQ(ev3->first, 5);

    EXPECT_EQ(c.debug_target_t1_size(), 0u) << "p must decrease on B2 hit";
    EXPECT_EQ(c.debug_list_tag(3), ARC_T2);
    EXPECT_EQ(c.debug_t2().front(), 3);

    check_arc_structure(c, "unit");
}

TEST_F(ARCTest, T1FullMissErasesWithoutGhost)
{
    init(3);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    // T1 = [3 2 1], p = 0

    auto ev = c.insert(4, 40);

    ASSERT_TRUE(ev.has_value());
    EXPECT_EQ(ev->first, 1);
    EXPECT_EQ(ev->second, 10);

    // 1 was erased without a ghost 
    EXPECT_EQ(c.debug_list_tag(1), ARC_UNKNOWN);
    EXPECT_FALSE(c.debug_in_ghost(1));

    EXPECT_EQ(c.debug_t1().size(), 3u);
    EXPECT_EQ(c.debug_t1().front(), 4);
    check_arc_structure(c, "unit");
}




// --- long sequence ---



using ARCInv = CacheInvTest<ARCCache<int, int>>;

TEST_P(ARCInv, InvTest)
{
    const std::size_t CAP       = GetParam().cap;
    const int         KEY_RANGE = GetParam().key_range;
    const int         OPS       = GetParam().ops;

    auto& c = *cache;

    std::mt19937 rng(GetParam().seed);
    std::uniform_int_distribution<int> kd(0, KEY_RANGE - 1);
    std::uniform_int_distribution<int> od(0, 99);

    std::unordered_map<int, int> access_count;
    std::unordered_map<int, int> saved_count;

    std::size_t cnt_ins_new      = 0;
    std::size_t cnt_ins_existing = 0;
    std::size_t cnt_ins_evict    = 0;
    std::size_t cnt_get_hit      = 0;
    std::size_t cnt_get_miss     = 0;
    std::size_t cnt_ext_hit      = 0;
    std::size_t cnt_ext_miss     = 0;
    std::size_t cnt_contains     = 0;

    auto check_semantic = [&](const std::string& ctx, int k)
    {
        const int tag_after = c.debug_list_tag(k);
        if (tag_after == ARC_UNKNOWN) return;

        const int cnt = access_count[k];

        if (cnt <= 1)
        {
            EXPECT_TRUE(tag_after == ARC_T1 || tag_after == ARC_B1)
                << ctx << " key " << k << " seen " << cnt
                << " time(s), tagged " << tag_name(tag_after);
        }

        else
        {
            EXPECT_TRUE(tag_after == ARC_T2 || tag_after == ARC_B2)
                << ctx << " key " << k << " seen " << cnt
                << " times, tagged " << tag_name(tag_after);
        }
    };

    for (int i = 0; i < OPS; i++)
    {
        const int k = kd(rng);
        const int o = od(rng);

        const std::string ctx = "step=" + std::to_string(i) +
                                " op=" + std::to_string(o) +
                                " key=" + std::to_string(k);

        const int  tag_before     = c.debug_list_tag(k);
        const bool was_resident   = (tag_before == ARC_T1 || tag_before == ARC_T2);
        const bool was_in_b1      = (tag_before == ARC_B1);
        const bool was_in_b2      = (tag_before == ARC_B2);

        if (o < 50)
        {
            const std::size_t before_res   = c.debug_t1().size() + c.debug_t2().size();
            const bool        was_in_ghost = c.debug_in_ghost(k);

            auto ev = c.insert(k, i);

            if (was_resident)
            {
                cnt_ins_existing++;

                EXPECT_FALSE(ev.has_value()) << ctx;
                EXPECT_EQ(c.debug_t1().size() + c.debug_t2().size(), before_res) << ctx;

                ASSERT_FALSE(c.debug_t2().empty()) << ctx;
                EXPECT_EQ(c.debug_t2().front(), k) << ctx;

                EXPECT_EQ(c.debug_list_tag(k), ARC_T2) << ctx;
            }

            else if (was_in_b1 || was_in_b2)
            {
                cnt_ins_evict++;

                const std::size_t after_res = c.debug_t1().size() + c.debug_t2().size();
                EXPECT_TRUE(after_res == before_res || after_res == before_res + 1) << ctx;
                EXPECT_LE(after_res, CAP) << ctx;

                ASSERT_FALSE(c.debug_t2().empty()) << ctx;
                EXPECT_EQ(c.debug_t2().front(), k) << ctx;

                EXPECT_EQ(c.debug_list_tag(k), ARC_T2) << ctx;
                EXPECT_TRUE(c.contains(k)) << ctx;
            }

            else
            {
                cnt_ins_new++;
                const std::size_t after_res = c.debug_t1().size() + c.debug_t2().size();
                EXPECT_LE(after_res, CAP) << ctx;
                EXPECT_TRUE(after_res == before_res || after_res == before_res + 1) << ctx;
                EXPECT_TRUE(c.contains(k)) << ctx;

                const int new_tag = c.debug_list_tag(k);
                EXPECT_TRUE(new_tag == ARC_T1 || new_tag == ARC_T2) << ctx;

                if (new_tag == ARC_T1)
                {
                    ASSERT_FALSE(c.debug_t1().empty()) << ctx;
                    EXPECT_EQ(c.debug_t1().front(), k) << ctx;
                }
                else
                {
                    ASSERT_FALSE(c.debug_t2().empty()) << ctx;
                    EXPECT_EQ(c.debug_t2().front(), k) << ctx;
                }
            }

            // --- semantic counter ---
            // accessed once == T1 or B1
            // accessed >1 times == T2 or B2
            if (was_in_ghost)
            {
                // reinsert after extract: restore previous counter, do not increment
                auto sit = saved_count.find(k);
                if (sit != saved_count.end())
                {
                    access_count[k] = sit->second;
                    saved_count.erase(sit);
                }


                else
                {
                    access_count[k] = 1;
                }
            }
            else if (tag_before == ARC_UNKNOWN)
            {
                // start a fresh history
                access_count[k] = 1;
            }

            else // hit
            {
                access_count[k]++;
            }

            check_semantic(ctx, k);
        }

        else if (o < 80)
        {
            const std::size_t before_t1 = c.debug_t1().size();
            const std::size_t before_t2 = c.debug_t2().size();

            auto v = c.get(k);

            if (v.has_value())
            {
                cnt_get_hit++;

                EXPECT_TRUE(c.contains(k)) << ctx;
                EXPECT_EQ(c.debug_t1().size() + c.debug_t2().size(), before_t1 + before_t2) << ctx;

                ASSERT_FALSE(c.debug_t2().empty()) << ctx;
                EXPECT_EQ(c.debug_t2().front(), k) << ctx;
                EXPECT_EQ(c.debug_list_tag(k), ARC_T2) << ctx;
                EXPECT_LE(c.debug_t1().size(), before_t1) << ctx;

                access_count[k]++;
            }
            else
            {
                cnt_get_miss++;

                EXPECT_FALSE(c.contains(k)) << ctx;
                EXPECT_EQ(c.debug_t1().size(), before_t1) << ctx;
                EXPECT_EQ(c.debug_t2().size(), before_t2) << ctx;

                if (tag_before == ARC_UNKNOWN)
                {
                    access_count[k] = 0;
                }
            }

            check_semantic(ctx, k);
        }

        else if (o < 95)
        {
            const std::size_t before_res = c.debug_t1().size() + c.debug_t2().size();

            const int prior_tag = tag_before;

            auto v = c.extract(k);

            if (v.has_value())
            {
                cnt_ext_hit++;

                EXPECT_FALSE(c.contains(k))          << ctx;
                EXPECT_FALSE(c.get(k).has_value())   << ctx;

                EXPECT_EQ(c.debug_t1().size() + c.debug_t2().size(), before_res - 1) << ctx;
                EXPECT_TRUE(c.debug_in_ghost(k))     << ctx;

                const bool saved_t2 = c.debug_ghost_is_t2(k);
                if (prior_tag == ARC_T1) EXPECT_FALSE(saved_t2) << ctx;
                if (prior_tag == ARC_T2) EXPECT_TRUE(saved_t2)  << ctx;

                // save counter for a possible later reinsert
                saved_count[k] = access_count[k];
            }

            else
            {
                cnt_ext_miss++;

                

                EXPECT_EQ(c.debug_t1().size() + c.debug_t2().size(), before_res) << ctx;
            }
        }

        else
        {
            const std::size_t before_t1 = c.debug_t1().size();
            const std::size_t before_t2 = c.debug_t2().size();
            const std::size_t before_b1 = c.debug_b1().size();
            const std::size_t before_b2 = c.debug_b2().size();
            const std::size_t before_vals = c.debug_values().size();

            cnt_contains++;
            c.contains(k);

            EXPECT_EQ(c.debug_t1().size(),    before_t1)   << ctx;
            EXPECT_EQ(c.debug_t2().size(),    before_t2)   << ctx;
            EXPECT_EQ(c.debug_b1().size(),    before_b1)   << ctx;
            EXPECT_EQ(c.debug_b2().size(),    before_b2)   << ctx;
            EXPECT_EQ(c.debug_values().size(), before_vals) << ctx;
        }

        check_arc_structure(c, ctx);
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

    check_arc_structure(c, "final");
}

INSTANTIATE_TEST_SUITE_P(ArcSizes, ARCInv, StandardInvParams());

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

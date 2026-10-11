#include <gtest/gtest.h>
#include "twoq_cache.h"
#include "cache_test_common.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using cache::TwoQCache;


constexpr int TAG_NOT_FOUND = -1;
constexpr int TAG_NONE      = 0;
constexpr int TAG_Q1        = 1;
constexpr int TAG_Q2        = 2;
constexpr int TAG_GHOST     = 3;




const char* tag_name(int tag)
{
    switch (tag)
    {
        case TAG_NOT_FOUND: return "NOT_FOUND";
        case TAG_Q1:        return "Q1";
        case TAG_Q2:        return "Q2";
        case TAG_GHOST:     return "GHOST";
    }
    return "?";
}




class TwoQTest : public ::testing::Test
{
protected:
    std::unique_ptr<TwoQCache<int, int>> cache;

    void init(std::size_t capacity)
    {
        cache = std::make_unique<TwoQCache<int, int>>(capacity);
    }

    void TearDown() override
    {
        if (::testing::Test::HasFailure() && cache)
            cache->dump(std::cerr);
    }
};




void check_twoq_structure(const TwoQCache<int, int>& c, const std::string& what)
{
    SCOPED_TRACE(what);

    const auto& q1    = c.debug_q1();
    const auto& q2    = c.debug_q2();
    const auto& ghost = c.debug_ghost();
    const auto& vals  = c.debug_values();

    const std::size_t cap = c.capacity();

    // target sizes
    EXPECT_EQ(c.debug_target_q1_size(),    std::max<std::size_t>(1, cap / 4)) << "target_q1_size broken";
    EXPECT_EQ(c.debug_target_ghost_size(), std::max<std::size_t>(1, cap / 2)) << "target_ghost_size broken";


    EXPECT_LE(q1.size() + q2.size(), cap) << "resident set > capacity";
    EXPECT_LE(ghost.size(), c.debug_target_ghost_size()) << "ghost > target_ghost_size";

    // values mirror resident set
    EXPECT_EQ(vals.size(), q1.size() + q2.size()) << "values size != Q1 + Q2";

    // no duplicates, tag, residency
    std::unordered_set<int> all_seen;

    auto walk = [&](const std::list<int>& lst, int expected_tag, bool resident)
    {
        std::unordered_set<int> local;
        for (int k : lst)
        {
            EXPECT_TRUE(local.insert(k).second) << "duplicate in list " << tag_name(expected_tag) << ": " << k;
            EXPECT_TRUE(all_seen.insert(k).second) << "key " << k << " appears in more than one list";

            EXPECT_EQ(c.debug_list_tag(k), expected_tag) << "tag mismatch for key " << k;

            EXPECT_EQ(c.contains(k), resident) << "residency mismatch for " << k;

            const bool has_value = vals.find(k) != vals.end();

            if (resident)
            {
                EXPECT_TRUE(has_value) << "resident key " << k << " has no value";


                if (has_value)
                {
                    EXPECT_TRUE(vals.at(k)) << "null value for resident " << k;
                }
            }

            else
            {
                EXPECT_FALSE(has_value) << "ghost key " << k << " has a value";
            }
        }
    };

    walk(q1,    TAG_Q1,    true);
    walk(q2,    TAG_Q2,    true);
    walk(ghost, TAG_GHOST, false);

    // every element_info appears in exactly one list
    EXPECT_EQ(all_seen.size(), c.debug_infos_size()) << "element_infos has keys missing from all lists";


    EXPECT_TRUE(c.debug_iterators()) << "element_infos[k].it is not a valid iterator to k";

    // values dont contain ghost keys
    for (const auto& [k, _] : vals)
    {
        const int tag = c.debug_list_tag(k);
        EXPECT_TRUE(tag == TAG_Q1 || tag == TAG_Q2)
            << "value for non-resident key " << k
            << " (tag " << tag_name(tag) << ")";
    }
}


static void insert_n(TwoQCache<int, int>& c, int n)
{
    for (int i = 1; i <= n; i++) c.insert(i, i * 10);
}


// --- unit ---


TEST_F(TwoQTest, InsertIntoEmptyInQ1)
{
    init(4);
    auto& c = *cache;

    auto ev = c.insert(1, 10);

    EXPECT_FALSE(ev.has_value());
    EXPECT_TRUE(c.contains(1));
    EXPECT_EQ(c.debug_list_tag(1), TAG_Q1);
    EXPECT_EQ(c.debug_q1().front(), 1);
    EXPECT_EQ(c.debug_q1().size(), 1u);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, InsertExistingInQ1MovesToQ2)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    ASSERT_EQ(c.debug_list_tag(1), TAG_Q1);

    auto ev = c.insert(1, 11);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(c.debug_q1().size(), 0u);
    EXPECT_EQ(c.debug_list_tag(1), TAG_Q2);
    EXPECT_EQ(c.debug_q2().front(), 1);
    EXPECT_EQ(*c.get(1), 11);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, InsertExistingInQ2StaysInQ2)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    c.get(1);
    ASSERT_EQ(c.debug_list_tag(1), TAG_Q2);

    auto ev = c.insert(1, 11);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(c.debug_list_tag(1), TAG_Q2);
    EXPECT_EQ(c.debug_q2().front(), 1);
    EXPECT_EQ(*c.get(1), 11);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, GetQ1HitMovesToQ2)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    ASSERT_EQ(c.debug_list_tag(1), TAG_Q1);

    auto v = c.get(1);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 10);
    EXPECT_EQ(c.debug_list_tag(1), TAG_Q2);
    EXPECT_EQ(c.debug_q1().size(), 1u);
    EXPECT_EQ(c.debug_q1().front(), 2);
    EXPECT_EQ(c.debug_q2().front(), 1);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, GetQ2HitMovesToFront)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.get(1);
    c.get(2);
    ASSERT_EQ(c.debug_q2().front(), 2);
    ASSERT_EQ(c.debug_q2().back(), 1);

    auto v = c.get(1);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 10);
    EXPECT_EQ(c.debug_q2().front(), 1);
    EXPECT_EQ(c.debug_q2().back(), 2);

    check_twoq_structure(c, "unit");
}


TEST_F(TwoQTest, ExtractQ1SavesAsQ1)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    ASSERT_EQ(c.debug_list_tag(1), TAG_Q1);

    auto v = c.extract(1);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 10);
    EXPECT_FALSE(c.contains(1));
    EXPECT_TRUE(c.debug_in_ghost(1));
    EXPECT_FALSE(c.debug_ghost_is_q2(1));
    EXPECT_EQ(c.debug_q1().size(), 0u);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, ExtractQ2SavesAsQ2)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    c.get(1);
    ASSERT_EQ(c.debug_list_tag(1), TAG_Q2);

    auto v = c.extract(1);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 10);
    EXPECT_FALSE(c.contains(1));
    EXPECT_TRUE(c.debug_in_ghost(1));
    EXPECT_TRUE(c.debug_ghost_is_q2(1));
    EXPECT_EQ(c.debug_q2().size(), 0u);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, Contains)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);

    EXPECT_TRUE(c.contains(1));
    EXPECT_TRUE(c.contains(2));
    EXPECT_FALSE(c.contains(999));

    EXPECT_EQ(c.debug_q1().size(), 2u);
    EXPECT_EQ(c.debug_q2().size(), 0u);
    EXPECT_EQ(c.debug_ghost().size(), 0u);

    check_twoq_structure(c, "unit");
}



TEST_F(TwoQTest, ReinsertAfterExtractQ1ReturnsToQ1)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    c.extract(1);
    ASSERT_TRUE(c.debug_in_ghost(1));
    ASSERT_FALSE(c.debug_ghost_is_q2(1));

    c.insert(1, 11);

    EXPECT_EQ(c.debug_list_tag(1), TAG_Q1);
    EXPECT_EQ(c.debug_q1().front(), 1);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, ReinsertAfterExtractQ2ReturnsToQ2)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    c.get(1);
    c.extract(1);
    ASSERT_TRUE(c.debug_in_ghost(1));
    ASSERT_TRUE(c.debug_ghost_is_q2(1));

    c.insert(1, 11);

    EXPECT_EQ(c.debug_list_tag(1), TAG_Q2);
    EXPECT_EQ(c.debug_q2().front(), 1);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, ListGhostHitPromotesToQ2)
{
    init(8); // target_q1 = 2, target_ghost = 4
    auto& c = *cache;

    insert_n(c, 9);

    ASSERT_EQ(c.debug_list_tag(1), TAG_GHOST);
    ASSERT_FALSE(c.debug_q1().empty());
    const int q1_back = c.debug_q1().back();

    auto ev = c.insert(1, 100);

    ASSERT_TRUE(ev.has_value());
    EXPECT_EQ(ev->first, q1_back);
    EXPECT_EQ(c.debug_list_tag(1), TAG_Q2);
    EXPECT_EQ(c.debug_q2().front(), 1);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, Q1OverflowPushesToGhost)
{
    init(8); // target_q1 = 2, target_ghost = 4
    auto& c = *cache;

    insert_n(c, 9);

    EXPECT_EQ(c.debug_list_tag(1), TAG_GHOST);
    EXPECT_EQ(c.debug_ghost().size(), 1u);
    EXPECT_EQ(c.debug_ghost().front(), 1);

    EXPECT_EQ(c.debug_list_tag(2), TAG_Q1);
    EXPECT_EQ(c.debug_q1().size(), 8u);

    check_twoq_structure(c, "unit");
}



TEST_F(TwoQTest, GhostTrimRespectsTargetSize)
{
    init(8); // target_q1 = 2, target_ghost = 4
    auto& c = *cache;

    insert_n(c, 20);

    EXPECT_EQ(c.debug_ghost().size(), 4u);

    EXPECT_EQ(c.debug_list_tag(12), TAG_GHOST);
    EXPECT_EQ(c.debug_list_tag(11), TAG_GHOST);
    EXPECT_EQ(c.debug_list_tag(10), TAG_GHOST);
    EXPECT_EQ(c.debug_list_tag(9),  TAG_GHOST);

    // earlier ones
    EXPECT_EQ(c.debug_list_tag(1), TAG_NOT_FOUND);
    EXPECT_EQ(c.debug_list_tag(2), TAG_NOT_FOUND);

    check_twoq_structure(c, "unit");
}


// --- edge ---



// --- long sequence ---


using TwoQInv = CacheInvTest<TwoQCache<int, int>>;

TEST_P(TwoQInv, InvTest)
{
    const std::size_t CAP       = GetParam().cap;
    const int         KEY_RANGE = GetParam().key_range;
    const int         OPS       = GetParam().ops;

    auto& c = *cache;

    EXPECT_EQ(c.debug_target_q1_size(),    std::max<std::size_t>(1, CAP / 4));
    EXPECT_EQ(c.debug_target_ghost_size(), std::max<std::size_t>(1, CAP / 2));

    std::mt19937 rng(1234);
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
        if (tag_after == TAG_NOT_FOUND) return;

        const int cnt = access_count[k];
        
        if (cnt <= 1)
        {
            EXPECT_TRUE(tag_after == TAG_Q1 || tag_after == TAG_GHOST)
                << ctx << " key " << k << " seen " << cnt
                << " time(s), tagged " << tag_name(tag_after);
        }

        else
        {
            EXPECT_TRUE(tag_after == TAG_Q2)
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
        const bool was_resident   = (tag_before == TAG_Q1 || tag_before == TAG_Q2);
        const bool was_list_ghost = (tag_before == TAG_GHOST);

        const bool was_in_sys = c.debug_in_ghost(k);
        const bool sys_was_q2 = c.debug_ghost_is_q2(k);

        if (o < 50)
        {
            const std::size_t before_res = c.debug_q1().size() + c.debug_q2().size();

            // expected target list
            const bool expect_q2 =
                was_resident ||
                was_list_ghost ||
                (was_in_sys && sys_was_q2);

            auto ev = c.insert(k, i);

            if (was_resident)
            {
                cnt_ins_existing++;
                EXPECT_FALSE(ev.has_value()) << ctx;
                EXPECT_EQ(c.debug_q1().size() + c.debug_q2().size(), before_res) << ctx;
                ASSERT_FALSE(c.debug_q2().empty()) << ctx;
                EXPECT_EQ(c.debug_q2().front(), k) << ctx;
                EXPECT_EQ(c.debug_list_tag(k), TAG_Q2) << ctx;
            }

            else
            {
                if (before_res == CAP)
                {
                    cnt_ins_evict++;
                    ASSERT_TRUE(ev.has_value()) << ctx;




                    EXPECT_FALSE(c.contains(ev->first)) << ctx;
                    EXPECT_EQ(c.debug_q1().size() + c.debug_q2().size(), CAP) << ctx;
                }

                else
                {
                    cnt_ins_new++;
                    EXPECT_FALSE(ev.has_value()) << ctx;
                    EXPECT_EQ(c.debug_q1().size() + c.debug_q2().size(), before_res + 1) << ctx;
                }

                EXPECT_TRUE(c.contains(k)) << ctx;

                if (expect_q2)
                {
                    EXPECT_EQ(c.debug_list_tag(k), TAG_Q2) << ctx;

                    ASSERT_FALSE(c.debug_q2().empty()) << ctx;

                    EXPECT_EQ(c.debug_q2().front(), k) << ctx;
                }

                else
                {
                    EXPECT_EQ(c.debug_list_tag(k), TAG_Q1) << ctx;

                    ASSERT_FALSE(c.debug_q1().empty()) << ctx;

                    EXPECT_EQ(c.debug_q1().front(), k) << ctx;
                }
            }

            // --- semantic counter ---
            if (was_in_sys)
            {
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

            else if (tag_before == TAG_NOT_FOUND)
            {
                access_count[k] = 1;
            }

            else
            {
                // resident hit or list ghost hit
                access_count[k]++;
            }

            check_semantic(ctx, k);
        }

        else if (o < 80)
        {
            const std::size_t before_q1    = c.debug_q1().size();
            const std::size_t before_q2    = c.debug_q2().size();
            const std::size_t before_ghost = c.debug_ghost().size();

            auto v = c.get(k);

            if (v.has_value())
            {
                cnt_get_hit++;

                EXPECT_TRUE(c.contains(k)) << ctx;
                EXPECT_EQ(c.debug_q1().size() + c.debug_q2().size(), before_q1 + before_q2) << ctx;
                EXPECT_EQ(c.debug_ghost().size(), before_ghost) << ctx;

                EXPECT_EQ(c.debug_list_tag(k), TAG_Q2) << ctx;
                ASSERT_FALSE(c.debug_q2().empty()) << ctx;
                EXPECT_EQ(c.debug_q2().front(), k) << ctx;

                access_count[k]++;
            }

            else
            {
                cnt_get_miss++;

                EXPECT_FALSE(c.contains(k)) << ctx;

                EXPECT_EQ(c.debug_q1().size(),    before_q1)    << ctx;
                EXPECT_EQ(c.debug_q2().size(),    before_q2)    << ctx;
                EXPECT_EQ(c.debug_ghost().size(), before_ghost) << ctx;

                // get on ghost is a miss: counter unchanged
                // get on unknown key: reset
                if (tag_before == TAG_NOT_FOUND)
                {
                    access_count[k] = 0;
                }
            }

            check_semantic(ctx, k);
        }

        else if (o < 95)
        {
            const std::size_t before_res = c.debug_q1().size() + c.debug_q2().size();

            const int prior_tag = tag_before;

            auto v = c.extract(k);

            if (v.has_value())
            {
                cnt_ext_hit++;

                EXPECT_FALSE(c.contains(k))<< ctx;
                EXPECT_EQ(c.debug_q1().size() + c.debug_q2().size(), before_res - 1) << ctx;

                ASSERT_TRUE(c.debug_in_ghost(k)) << ctx;

                const bool saved_q2 = c.debug_ghost_is_q2(k);

                if (prior_tag == TAG_Q1)
                {
                    EXPECT_FALSE(saved_q2) << ctx;
                }

                else if (prior_tag == TAG_Q2)
                {
                    EXPECT_TRUE(saved_q2) << ctx;

                }

                // save counter for a possible later reinsert
                saved_count[k] = access_count[k];
            }

            else
            {
                cnt_ext_miss++;

                EXPECT_EQ(c.debug_q1().size() + c.debug_q2().size(), before_res) << ctx;
            }
        }

        else
        {
            const std::size_t before_q1    = c.debug_q1().size();
            const std::size_t before_q2    = c.debug_q2().size();
            const std::size_t before_ghost = c.debug_ghost().size();

            const std::size_t before_vals  = c.debug_values().size();

            cnt_contains++;

            c.contains(k);

            EXPECT_EQ(c.debug_q1().size(),     before_q1)    << ctx;
            EXPECT_EQ(c.debug_q2().size(),     before_q2)    << ctx;
            EXPECT_EQ(c.debug_ghost().size(),  before_ghost) << ctx;

            EXPECT_EQ(c.debug_values().size(), before_vals)  << ctx;
        }


        check_twoq_structure(c, ctx);


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

    check_twoq_structure(c, "final");
}

INSTANTIATE_TEST_SUITE_P(TwoQSizes, TwoQInv, StandardInvParams());

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

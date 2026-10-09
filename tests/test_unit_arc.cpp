#include <gtest/gtest.h>
#include "arc_cache.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>






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


class DumpOnFailure
{
public:
    explicit DumpOnFailure(const ARCCache<int, int>& cache) : m_cache(cache) {}

    ~DumpOnFailure()
    {
        if (!::testing::Test::HasFailure() || s_already_reported) return;
        s_already_reported = true;
        m_cache.dump(std::cerr);
    }

private:
    const ARCCache<int, int>& m_cache;
    static inline bool s_already_reported = false;
};



void check_arc_structure(const ARCCache<int, int>& c, const std::string& what)
{
    SCOPED_TRACE(what);

    const auto& t1    = c.debug_t1();
    const auto& t2    = c.debug_t2();
    const auto& b1    = c.debug_b1();
    const auto& b2    = c.debug_b2();
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
                EXPECT_NE(vals.find(k), vals.end()) << "resident key " << k << " has no value";
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

    // every element_infos entry appears in exactly one list
    std::size_t num_infos = 0;
    for (int k : c.debug_all_keys())
    {
        num_infos++;

        EXPECT_TRUE(all_seen.count(k) > 0) << "key " << k << " in element_infos but not in any list";
    }

    EXPECT_EQ(num_infos, all_seen.size()) << "element_infos size does not match total keys in lists";

    // values contain elements keys only in T1 / T2
    for (const auto& [k, _] : vals)
    {
        const int tag = c.debug_list_tag(k);
        EXPECT_TRUE(tag == ARC_T1 || tag == ARC_T2)
            << "value for non-resident key " << k
            << " (tag " << tag_name(tag) << ")";
    }
}



TEST(ARC, InvTest)
{
    constexpr std::size_t CAP       = 1024;
    constexpr int         KEY_RANGE = 4096;
    constexpr int         OPS       = 100000;

    ARCCache<int, int> c(CAP);

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

    for (int i = 0; i < OPS; i++)
    {
        DumpOnFailure guard(c);

        const int k = kd(rng);
        const int o = od(rng);

        const std::string ctx = "step=" + std::to_string(i) +
                                " op=" + std::to_string(o) +
                                " key=" + std::to_string(k);

        const int  tag          = c.debug_list_tag(k);
        const bool was_resident = (tag == ARC_T1 || tag == ARC_T2);
        const bool was_in_b1    = (tag == ARC_B1);
        const bool was_in_b2    = (tag == ARC_B2);

        if (o < 50)
        {
            const std::size_t before_res = c.debug_t1().size() + c.debug_t2().size();

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

                EXPECT_EQ(c.debug_t1().size() + c.debug_t2().size(), before_res) << ctx;

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
            }
            else
            {
                cnt_get_miss++;

                EXPECT_FALSE(c.contains(k)) << ctx;
                EXPECT_EQ(c.debug_t1().size(), before_t1) << ctx;
                EXPECT_EQ(c.debug_t2().size(), before_t2) << ctx;
            }
        }

        else if (o < 95)
        {
            const std::size_t before_res = c.debug_t1().size() + c.debug_t2().size();

            const int prior_tag = tag;

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

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include <gtest/gtest.h>
#include "twoq_cache.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>




constexpr int TAG_NOT_FOUND = 0;
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






class DumpOnFailure
{
public:
    explicit DumpOnFailure(const TwoQCache<int, int>& cache) : m_cache(cache) {}

    ~DumpOnFailure()
    {
        if (!::testing::Test::HasFailure() || s_already_reported) return;
        s_already_reported = true;
        m_cache.dump(std::cerr);
    }

private:
    const TwoQCache<int, int>& m_cache;
    static inline bool s_already_reported = false;
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


TEST(twoQ, InvTest)
{
    constexpr std::size_t CAP       = 1024;
    constexpr int         KEY_RANGE = 4096;
    constexpr int         OPS       = 100000;

    TwoQCache<int, int> c(CAP);

    EXPECT_EQ(c.debug_target_q1_size(),    std::max<std::size_t>(1, CAP / 4));
    EXPECT_EQ(c.debug_target_ghost_size(), std::max<std::size_t>(1, CAP / 2));

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
            }

            else
            {
                cnt_get_miss++;

                EXPECT_FALSE(c.contains(k)) << ctx;

                EXPECT_EQ(c.debug_q1().size(),    before_q1)    << ctx;
                EXPECT_EQ(c.debug_q2().size(),    before_q2)    << ctx;
                EXPECT_EQ(c.debug_ghost().size(), before_ghost) << ctx;
            }
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

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

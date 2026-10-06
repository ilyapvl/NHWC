#include <gtest/gtest.h>
#include "arc_cache.h"

#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>





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
    const auto& infos = c.debug_element_infos();
    const auto& vals  = c.debug_values();

    const std::size_t cap = c.capacity();

    // arc lists sizes
    EXPECT_LE(t1.size() + t2.size(), cap)   << "|T1| + |T2| > capacity (resident set too large)";
    EXPECT_LE(t1.size() + b1.size(), cap)   << "|T1| + |B1| > capacity";

    EXPECT_LE(t1.size() + t2.size() + b1.size() + b2.size(), 2 * cap) << "total (T1+T2+B1+B2) > 2 * capacity";
    
    EXPECT_LE(c.debug_target_t1_size(), cap) << "target_t1_size > capacity";

    // values mirror resident set
    EXPECT_EQ(vals.size(), t1.size() + t2.size()) << "values size != |T1| + |T2|";

    // each list has no duplicates and iterators are valid
    std::unordered_set<int> all_seen;

    auto walk = [&](const std::list<int>& lst, ARCCache<int, int>::List tag, bool resident)
    {
        std::unordered_set<int> local;
        for (auto it = lst.begin(); it != lst.end(); ++it)
        {
            EXPECT_TRUE(local.insert(*it).second)
                << "duplicate in list tag=" << static_cast<int>(tag)
                << " key=" << *it;


            EXPECT_TRUE(all_seen.insert(*it).second) << "key " << *it << " appears in more than one list";

            auto iit = infos.find(*it);
            ASSERT_NE(iit, infos.end()) << "key " << *it << " in a list but not in element_infos";


            EXPECT_EQ(iit->second.list, tag) << "list tag mismatch for " << *it;
            EXPECT_EQ(iit->second.resident, resident) << "resident flag mismatch for " << *it;
            EXPECT_EQ(iit->second.it, it) << "iterator mismatch for " << *it;

            if (resident)
            {
                EXPECT_NE(vals.find(*it), vals.end()) << "resident key " << *it << " has no value";
            }
            else
            {
                EXPECT_EQ(vals.find(*it), vals.end()) << "ghost key " << *it << " has a value";
            }
        }
    };

    walk(t1, ARCCache<int, int>::List::T1, true);
    walk(t2, ARCCache<int, int>::List::T2, true);
    walk(b1, ARCCache<int, int>::List::B1, false);
    walk(b2, ARCCache<int, int>::List::B2, false);

    // every element_infos entry appears in exactly one list
    EXPECT_EQ(all_seen.size(), infos.size()) << "element_infos contains a key missing from all four lists";

    for (const auto& [k, info] : infos)
    {
        EXPECT_TRUE(all_seen.count(k) > 0) << "key " << k << " in element_infos but not in any list";

        if (info.list == ARCCache<int, int>::List::T1 || info.list == ARCCache<int, int>::List::T2)
        {
            EXPECT_TRUE(info.resident) << "T1/T2 key " << k << " not resident";
        }

        else
        {
            EXPECT_FALSE(info.resident) << "B1/B2 key " << k << " marked resident";
        }
    }

    // no values for ghost keys
    for (const auto& [k, _] : vals)
    {
        auto iit = infos.find(k);
        ASSERT_NE(iit, infos.end()) << "value for unknown key: " << k;
        EXPECT_TRUE(iit->second.resident) << "value for non-resident key: " << k;
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

        const bool was_resident = c.contains(k);

        bool was_in_b1 = false;
        bool was_in_b2 = false;
        {
            auto it = c.debug_element_infos().find(k);
            if (it != c.debug_element_infos().end())
            {
                was_in_b1 = (it->second.list == ARCCache<int, int>::List::B1);
                was_in_b2 = (it->second.list == ARCCache<int, int>::List::B2);
            }
        }

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
            }
            else if (was_in_b1 || was_in_b2)
            {
                cnt_ins_evict++;
                // ghost-hit path: key moves to T2
                EXPECT_EQ(c.debug_t1().size() + c.debug_t2().size(), before_res) << ctx;
                ASSERT_FALSE(c.debug_t2().empty()) << ctx;
                EXPECT_EQ(c.debug_t2().front(), k) << ctx;
                EXPECT_TRUE(c.contains(k)) << ctx;
            }
            else
            {
                // pure miss
                // resient may grow by 1 or stay the same
                cnt_ins_new++;

                const std::size_t after_res = c.debug_t1().size() + c.debug_t2().size();
                EXPECT_LE(after_res, CAP) << ctx;

                EXPECT_TRUE(after_res == before_res || after_res == before_res + 1) << ctx;
                EXPECT_TRUE(c.contains(k)) << ctx;

                // key must be at the front of T1 or T2
                const bool at_t1 = !c.debug_t1().empty() && c.debug_t1().front() == k;
                const bool at_t2 = !c.debug_t2().empty() && c.debug_t2().front() == k;
                EXPECT_TRUE(at_t1 || at_t2) << ctx;
            }
        }

        else if (o < 80)
        {
            const std::size_t before_res = c.debug_t1().size() + c.debug_t2().size();
            const std::size_t before_t1  = c.debug_t1().size();
            const std::size_t before_t2  = c.debug_t2().size();

            auto v = c.get(k);

            if (v.has_value())
            {
                cnt_get_hit++;
                EXPECT_TRUE(c.contains(k)) << ctx;
                EXPECT_EQ(c.debug_t1().size() + c.debug_t2().size(), before_res) << ctx;
                ASSERT_FALSE(c.debug_t2().empty()) << ctx;
                EXPECT_EQ(c.debug_t2().front(), k) << ctx;

                // get hit always moves the key from where it was to T2
                EXPECT_LE(c.debug_t1().size(), before_t1) << ctx;
                EXPECT_GE(c.debug_t2().size(), before_t2) << ctx;
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

            ARCCache<int, int>::List prior_list = ARCCache<int, int>::List::None;
            if (was_resident)
            {
                prior_list = c.debug_element_infos().at(k).list;
            }

            auto v = c.extract(k);

            if (v.has_value())
            {
                cnt_ext_hit++;
                EXPECT_FALSE(c.contains(k))          << ctx;
                EXPECT_FALSE(c.get(k).has_value())   << ctx;
                EXPECT_EQ(c.debug_t1().size() + c.debug_t2().size(), before_res - 1) << ctx;
                EXPECT_TRUE(c.debug_in_ghost(k))     << ctx;
                EXPECT_FALSE(c.debug_ghost_is_t2(k) && (prior_list == ARCCache<int, int>::List::T1))   << ctx << " ghost preserved wrong list";
                EXPECT_FALSE(!c.debug_ghost_is_t2(k) && (prior_list == ARCCache<int, int>::List::T2))  << ctx << " ghost preserved wrong list";
            }

            else
            {
                cnt_ext_miss++;
                EXPECT_EQ(c.debug_t1().size() + c.debug_t2().size(), before_res) << ctx;
            }
        }

        else
        {
            const std::size_t before_res = c.debug_t1().size() + c.debug_t2().size();
            const std::size_t before_t1  = c.debug_t1().size();
            const std::size_t before_t2  = c.debug_t2().size();
            const std::size_t before_b1  = c.debug_b1().size();
            const std::size_t before_b2  = c.debug_b2().size();

            cnt_contains++;
            c.contains(k);

            EXPECT_EQ(c.debug_t1().size(), before_t1) << ctx;
            EXPECT_EQ(c.debug_t2().size(), before_t2) << ctx;
            EXPECT_EQ(c.debug_b1().size(), before_b1) << ctx;
            EXPECT_EQ(c.debug_b2().size(), before_b2) << ctx;
            EXPECT_EQ(c.debug_t1().size() + c.debug_t2().size(), before_res) << ctx;
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

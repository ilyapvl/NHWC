#include <gtest/gtest.h>
#include "lru_cache.h"

#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>



class DumpOnFailure
{
public:
    explicit DumpOnFailure(const LRUCache<int, int>& cache) : m_cache(cache) {}

    ~DumpOnFailure()
    {
        if (!::testing::Test::HasFailure() || s_already_reported) return;
        s_already_reported = true;
        m_cache.dump(std::cerr);
    }

private:
    const LRUCache<int, int>& m_cache;
    static inline bool s_already_reported = false;
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



TEST(LRUEdge, CapacityZeroBypass)
{
    LRUCache<int, int> c(0);

    for (int i = 0; i < 500; i++)
    {
        DumpOnFailure guard(c);

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





TEST(LRU, InvTest)
{
    constexpr std::size_t CAP       = 1024;
    constexpr int         KEY_RANGE = 4096;
    constexpr int         OPS       = 100000;

    LRUCache<int, int> c(CAP);

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

    std::cerr << "[executed operations] ins_new=" << cnt_ins_new
          << " ins_existing=" << cnt_ins_existing
          << " ins_evict=" << cnt_ins_evict
          << " get_hit=" << cnt_get_hit
          << " get_miss=" << cnt_get_miss
          << " ext_hit=" << cnt_ext_hit
          << " ext_miss=" << cnt_ext_miss
          << " contains=" << cnt_contains << '\n';

    check_lru_structure(c, "final");
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

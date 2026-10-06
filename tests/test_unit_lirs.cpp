#include <gtest/gtest.h>
#include "lirs_cache.h"

#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

class DumpOnFailure
{
public:
    explicit DumpOnFailure(const LIRSCache<int, int>& cache) : m_cache(cache) {}

    ~DumpOnFailure()
    {
        if (!::testing::Test::HasFailure() || s_already_reported) return;
        s_already_reported = true;
        m_cache.dump(std::cerr);
    }

private:
    const LIRSCache<int, int>& m_cache;
    static inline bool s_already_reported = false;
};

std::size_t expected_lir_capacity(std::size_t capacity)
{
    if (capacity < 2) return 0;
    return capacity - std::max(LIRSCache<int, int>::MIN_HIR, capacity / LIRSCache<int, int>::LIR_HIR_RATIO);
}

std::size_t expected_hir_capacity(std::size_t capacity)
{
    return capacity - expected_lir_capacity(capacity);
}

void check_lirs_structure(const LIRSCache<int, int>& c, const std::string& what)
{
    SCOPED_TRACE(what);

    const auto& stack = c.debug_stack();
    const auto& queue = c.debug_queue();
    const auto& infos = c.debug_element_infos();
    const auto& vals  = c.debug_values();

    const std::size_t cap           = c.capacity();
    const std::size_t exp_lir_cap   = expected_lir_capacity(cap);
    const std::size_t exp_hir_cap   = expected_hir_capacity(cap);

    // lir_capacity matches the derived from LIR_HIR_RATIO / MIN_HIR
    EXPECT_EQ(c.debug_lir_capacity(), exp_lir_cap) << "lir_capacity formula broken for cap=" << cap;

    // counters and sizes
    EXPECT_LE(c.debug_resident_count(), cap)                  << "resident > capacity";
    EXPECT_LE(c.debug_lir_count(), c.debug_lir_capacity())    << "LIR > LIR capacity";
    EXPECT_LE(c.debug_lir_count(), c.debug_resident_count())  << "LIR > resident";
    EXPECT_EQ(c.debug_resident_count(), vals.size())          << "resident count vs values";
    EXPECT_LE(c.debug_ghost_size(), c.debug_ghost_capacity()) << "ghost > capacity";

    // every resident is either LIR or resident HIR, and resident HIRs are in the queue
    EXPECT_EQ(queue.size(), c.debug_resident_count() - c.debug_lir_count()) << "queue size != resident_count - lir_count";

    // ---- stack >= resident (every resident lives in the stack)
    EXPECT_GE(stack.size(), c.debug_resident_count()) << "stack smaller than resident set";

    // ---- stack upper bound from cut_stack: high_size = 3 * capacity
    EXPECT_LE(stack.size(), 3 * cap) << "stack > 3 * capacity";

    // ---- MIN_HIR guarantee: at least one HIR exists when capacity >= 2
    if (cap >= 2)
    {
        EXPECT_GE(exp_hir_cap, (LIRSCache<int, int>::MIN_HIR))
            << "target HIR capacity below MIN_HIR for cap=" << cap;
    }

    // ---- full-cache-specific guarantees
    if (c.debug_resident_count() == cap && cap >= 2)
    {
        EXPECT_LT(c.debug_lir_count(), cap)     << "full cache is all-LIR";
        EXPECT_FALSE(queue.empty())             << "full cache has empty HIR queue";
        EXPECT_GE(queue.size(), exp_hir_cap)    << "full cache: queue below target HIR capacity";
    }

    // ---- stack contents vs element_infos
    std::unordered_set<int> stack_set;
    stack_set.reserve(stack.size() * 2);
    for (int k : stack)
    {
        EXPECT_TRUE(stack_set.insert(k).second) << "duplicate in stack: " << k;

        auto it = infos.find(k);
        ASSERT_NE(it, infos.end()) << "stack key " << k << " missing from infos";
        EXPECT_TRUE(it->second.in_stack)      << "in_stack false for stack key " << k;
        EXPECT_EQ(*it->second.stack_it, k)    << "stack_it mismatch for " << k;
    }

    // ---- queue contents vs element_infos
    std::unordered_set<int> queue_set;
    queue_set.reserve(queue.size() * 2);
    for (int k : queue)
    {
        EXPECT_TRUE(queue_set.insert(k).second) << "duplicate in queue: " << k;

        auto it = infos.find(k);
        ASSERT_NE(it, infos.end()) << "queue key " << k << " missing from infos";
        EXPECT_TRUE(it->second.in_queue)      << "in_queue false for queue key " << k;
        EXPECT_TRUE(it->second.resident)      << "queue key " << k << " is not resident";
        EXPECT_FALSE(it->second.lir)          << "LIR key in HIR queue: " << k;
        EXPECT_EQ(*it->second.queue_it, k)    << "queue_it mismatch for " << k;
        EXPECT_NE(vals.find(k), vals.end())   << "queue key " << k << " has no value";
    }

    // ---- element_infos flags and counters
    std::size_t resident_cnt = 0;
    std::size_t lir_cnt      = 0;
    std::size_t in_stack_cnt = 0;
    std::size_t in_queue_cnt = 0;

    for (const auto& [k, info] : infos)
    {
        if (info.resident) resident_cnt++;
        if (info.lir)      lir_cnt++;
        if (info.in_stack) in_stack_cnt++;
        if (info.in_queue) in_queue_cnt++;

        EXPECT_EQ(info.in_stack, stack_set.count(k) > 0)        << "in_stack flag mismatch for " << k;
        EXPECT_EQ(info.in_queue, queue_set.count(k) > 0)        << "in_queue flag mismatch for " << k;
        EXPECT_EQ(info.resident, vals.find(k) != vals.end())    << "resident flag mismatch for " << k;

        if (info.lir)
        {
            EXPECT_TRUE(info.resident) << "LIR " << k << " is not resident";
            EXPECT_TRUE(info.in_stack) << "LIR " << k << " is not in stack";
        }

        if (info.resident)
        {
            auto vit = vals.find(k);
            ASSERT_NE(vit, vals.end()) << "resident " << k << " has no value";
            EXPECT_TRUE(vit->second)   << "null value for " << k;
        }
    }

    EXPECT_EQ(resident_cnt, c.debug_resident_count()) << "resident counter mismatch";
    EXPECT_EQ(lir_cnt,      c.debug_lir_count())      << "LIR counter mismatch";
    EXPECT_EQ(stack.size(), in_stack_cnt)             << "stack size vs in_stack flags";
    EXPECT_EQ(queue.size(), in_queue_cnt)             << "queue size vs in_queue flags";


    for (const auto& [k, _] : vals)
    {
        auto it = infos.find(k);
        ASSERT_NE(it, infos.end()) << "value without element_info: " << k;
        EXPECT_TRUE(it->second.resident) << "value for non-resident key: " << k;
    }
}


TEST(LIRS, InvTest)
{
    constexpr std::size_t CAP       = 1024;
    constexpr int         KEY_RANGE = 4096;
    constexpr int         OPS       = 100000;

    LIRSCache<int, int> c(CAP);

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
            const std::size_t before_size = c.debug_resident_count();
            const std::size_t before_lir  = c.debug_lir_count();

            auto ev = c.insert(k, i);

            if (existed)
            {
                cnt_ins_existing++;
                EXPECT_FALSE(ev.has_value()) << ctx;
                EXPECT_EQ(c.debug_resident_count(), before_size) << ctx;
            }
            else if (before_size == CAP)
            {
                cnt_ins_evict++;
                ASSERT_TRUE(ev.has_value()) << ctx;
                EXPECT_EQ(c.debug_resident_count(), CAP) << ctx;
                EXPECT_FALSE(c.contains(ev->first)) << ctx;
            }
            else
            {
                cnt_ins_new++;
                EXPECT_FALSE(ev.has_value()) << ctx;
                EXPECT_EQ(c.debug_resident_count(), before_size + 1) << ctx;
            }

            EXPECT_TRUE(c.contains(k)) << ctx;
            EXPECT_LE(c.debug_lir_count(), c.debug_lir_capacity()) << ctx;
            (void)before_lir;
        }

        else if (o < 40)
        {
            const std::size_t before_size = c.debug_resident_count();
            const std::size_t before_lir  = c.debug_lir_count();

            auto v = c.get(k);

            if (v.has_value())
            {
                cnt_get_hit++;
                EXPECT_TRUE(c.contains(k)) << ctx;
                EXPECT_EQ(c.debug_resident_count(), before_size) << ctx;
                EXPECT_LE(c.debug_lir_count(), c.debug_lir_capacity()) << ctx;
            }
            else
            {
                cnt_get_miss++;
                EXPECT_FALSE(c.contains(k)) << ctx;
                EXPECT_EQ(c.debug_resident_count(), before_size) << ctx;
                EXPECT_EQ(c.debug_lir_count(), before_lir) << ctx;
            }
        }

        else if (o < 95)
        {
            const std::size_t before_size = c.debug_resident_count();

            bool was_lir = false;
            if (c.contains(k))
            {
                was_lir = c.debug_element_infos().at(k).lir;
            }

            auto v = c.extract(k);

            if (v.has_value())
            {
                cnt_ext_hit++;
                EXPECT_FALSE(c.contains(k))          << ctx;
                EXPECT_FALSE(c.get(k).has_value())   << ctx;
                EXPECT_EQ(c.debug_resident_count(), before_size - 1) << ctx;
                EXPECT_TRUE(c.debug_in_ghost(k))     << ctx;
                EXPECT_EQ(c.debug_ghost_is_lir(k), was_lir) << ctx;
            }
            else
            {
                cnt_ext_miss++;
                EXPECT_EQ(c.debug_resident_count(), before_size) << ctx;
            }
        }

        else
        {
            const std::size_t before_size = c.debug_resident_count();
            const std::size_t before_lir  = c.debug_lir_count();

            cnt_contains++;
            c.contains(k);

            EXPECT_EQ(c.debug_resident_count(), before_size) << ctx;
            EXPECT_EQ(c.debug_lir_count(), before_lir) << ctx;
        }

        check_lirs_structure(c, ctx);
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

    check_lirs_structure(c, "final");
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

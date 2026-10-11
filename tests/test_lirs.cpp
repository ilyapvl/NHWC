#include <gtest/gtest.h>
#include "lirs_cache.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>


class LIRSTest : public ::testing::Test
{
protected:
    std::unique_ptr<LIRSCache<int, int>> cache;

    void init(std::size_t capacity)
    {
        cache = std::make_unique<LIRSCache<int, int>>(capacity);
    }

    void TearDown() override
    {
        if (::testing::Test::HasFailure() && cache)
            cache->dump(std::cerr);
    }
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

    // every LIR must be in the stack
    EXPECT_LE(c.debug_lir_count(), stack.size()) << "stack smaller than LIR set";

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


std::size_t resident_count(const LIRSCache<int, int>& c)
{
    return c.debug_resident_count();
}


static void fill_seq(LIRSCache<int, int>& c, int n)
{
    for (int i = 1; i <= n; i++) c.insert(i, i * 10);
}


// --- unit ---


TEST_F(LIRSTest, FirstInsertBecomesLir)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);

    EXPECT_TRUE(c.contains(1));
    EXPECT_EQ(resident_count(c), 1u);
    EXPECT_EQ(c.debug_lir_count(), 1u);
    EXPECT_TRUE(c.debug_element_infos().at(1).lir);



    check_lirs_structure(c, "unit");
}


TEST_F(LIRSTest, WarmupFillsLir)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);

    EXPECT_EQ(c.debug_lir_count(), 3u);
    EXPECT_EQ(c.debug_queue().size(), 0u);

    EXPECT_TRUE(c.debug_element_infos().at(1).lir);
    EXPECT_TRUE(c.debug_element_infos().at(2).lir);
    EXPECT_TRUE(c.debug_element_infos().at(3).lir);



    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, AfterWarmupNewKeyIsHir)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40); // hir

    EXPECT_EQ(c.debug_lir_count(), 3u);
    EXPECT_EQ(c.debug_queue().size(), 1u);
    EXPECT_FALSE(c.debug_element_infos().at(4).lir);
    EXPECT_TRUE(c.debug_element_infos().at(4).resident);


    check_lirs_structure(c, "unit");
}


TEST_F(LIRSTest, InsertExistingIsNoop)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 4);

    const auto before_size = resident_count(c);
    const auto before_lir  = c.debug_lir_count();

    auto ev = c.insert(2, 222);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(resident_count(c), before_size);
    EXPECT_EQ(c.debug_lir_count(), before_lir);
    EXPECT_EQ(*c.get(2), 222);

    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, GetLirHitMovesToStackFront)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 3);

    ASSERT_EQ(c.debug_stack().front(), 3);

    auto v = c.get(1);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 10);

    EXPECT_EQ(c.debug_stack().front(), 1);

    EXPECT_TRUE(c.debug_element_infos().at(1).lir);
    EXPECT_EQ(c.debug_lir_count(), 3u);

    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, GetHirInStackPromotesToLir)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40); // hir

    ASSERT_FALSE(c.debug_element_infos().at(4).lir);
    ASSERT_TRUE(c.debug_element_infos().at(4).in_stack);
    ASSERT_EQ(c.debug_queue().size(), 1u);

    auto v = c.get(4);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 40);

    // 4 is lir
    EXPECT_TRUE(c.debug_element_infos().at(4).lir);
    EXPECT_EQ(c.debug_lir_count(), c.debug_lir_capacity());

    // queue not empty
    EXPECT_EQ(c.debug_queue().size(), 1u);

    // 4 is no longer in the queue
    EXPECT_EQ(std::find(c.debug_queue().begin(), c.debug_queue().end(), 4), c.debug_queue().end());

    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, ExtractLirPreservesLirInGhost)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 3);

    ASSERT_TRUE(c.debug_element_infos().at(2).lir);

    auto v = c.extract(2);

    ASSERT_TRUE(v.has_value());
    EXPECT_FALSE(c.contains(2));
    EXPECT_TRUE(c.debug_in_ghost(2));
    EXPECT_TRUE(c.debug_ghost_is_lir(2));
    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, ExtractHirPreservesHirInGhost)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);               // HIR

    ASSERT_FALSE(c.debug_element_infos().at(4).lir);

    auto v = c.extract(4);

    ASSERT_TRUE(v.has_value());
    EXPECT_FALSE(c.contains(4));
    EXPECT_TRUE(c.debug_in_ghost(4));
    EXPECT_FALSE(c.debug_ghost_is_lir(4));
    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, ReinsertAfterExtractLirBecomesLir)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 3);                // 3, 2, 1 all LIR

    c.extract(2);
    ASSERT_TRUE(c.debug_ghost_is_lir(2));

    c.insert(2, 21);

    EXPECT_TRUE(c.contains(2));
    EXPECT_TRUE(c.debug_element_infos().at(2).lir);
    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, ReinsertAfterExtractHirBecomesHir)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);               // HIR

    c.extract(4);
    ASSERT_FALSE(c.debug_ghost_is_lir(4));

    c.insert(4, 41);

    EXPECT_TRUE(c.contains(4));
    EXPECT_FALSE(c.debug_element_infos().at(4).lir);
    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, EvictionPicksQueueBack)
{
    init(4);                       // lir_cap = 3
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);               // HIR in queue

    ASSERT_FALSE(c.debug_queue().empty());
    const int expected_victim = c.debug_queue().back();
    ASSERT_TRUE(c.contains(expected_victim));

    auto ev = c.insert(5, 50);

    ASSERT_TRUE(ev.has_value());
    EXPECT_EQ(ev->first, expected_victim);
    EXPECT_FALSE(c.contains(expected_victim));
    EXPECT_TRUE(c.contains(5));
    EXPECT_EQ(resident_count(c), 4u);
    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, ExtractLirThenReinsertPreservesCount)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 3);

    const auto before_lir = c.debug_lir_count();
    ASSERT_EQ(before_lir, 3u);

    c.extract(2);
    c.insert(2, 21);

    EXPECT_EQ(c.debug_lir_count(), before_lir);
    EXPECT_EQ(resident_count(c), 3u);
    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, ExtractHirThenReinsertDoesNotGrowLir)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40); // hir

    const auto before_lir = c.debug_lir_count();

    c.extract(4);
    c.insert(4, 41);

    EXPECT_EQ(c.debug_lir_count(), before_lir);
    EXPECT_FALSE(c.debug_element_infos().at(4).lir);

    check_lirs_structure(c, "unit");
}


TEST_F(LIRSTest, SecondAccessPromotesHirToLir)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);

    c.insert(4, 40);

    c.get(4); //to lir

    EXPECT_TRUE(c.debug_element_infos().at(4).lir);
    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, RepeatedHirAccessKeepsItResident)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);

    for (int i = 0; i < 5; i++)
    {
        auto v = c.get(4);

        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, 40);
    }

    EXPECT_TRUE(c.contains(4));

    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, ColdKeyFallsOutOfQueue)
{
    init(3);

    LIRSCache<int, int>& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);

    // 3 not resident

    EXPECT_FALSE(c.contains(3));

    EXPECT_TRUE(c.contains(4));

    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, TwoExtractsThenInsertKeepsCounts)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 3);

    c.extract(1);
    c.extract(2);

    EXPECT_EQ(resident_count(c), 1u);

    c.insert(1, 11);

    EXPECT_EQ(resident_count(c), 2u);
    EXPECT_EQ(c.debug_lir_count(), std::min<std::size_t>(2, c.debug_lir_capacity()));


    check_lirs_structure(c, "unit");
}






// --- m_last_promoted ---


TEST_F(LIRSTest, ExtractLirPromotesQueueFrontToLir)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);
    // stack: [4 3 2 1], queue: [4], lir: [1 2 3]

    ASSERT_EQ(c.debug_lir_count(), 3u);
    ASSERT_EQ(c.debug_queue().size(), 1u);
    ASSERT_FALSE(c.debug_element_infos().at(4).lir);

    // extract(2): lir_count drops 3 -> 2, restore promotes 4 back to lir
    auto v = c.extract(2);
    ASSERT_TRUE(v.has_value());

    EXPECT_TRUE(c.debug_element_infos().at(4).lir) << "queue front should have been promoted back to LIR";
    EXPECT_EQ(c.debug_lir_count(), c.debug_lir_capacity()) << "lir_count should be restored to capacity";
    EXPECT_EQ(c.debug_queue().size(), 0u) << "queue is empty: the only HIR was promoted";

    check_lirs_structure(c, "after extract");
}

TEST_F(LIRSTest, ExtractLirThenReinsertDemotesTheCompensatedKey)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);
    // lir: [1 2 3], queue: [4]

    // extract 2 >> 4 remembered in m_last_promoted
    ASSERT_TRUE(c.extract(2).has_value());
    ASSERT_TRUE(c.debug_element_infos().at(4).lir);

    // reinsert 2 >> 4 back to hir
    c.insert(2, 21);

    EXPECT_TRUE (c.debug_element_infos().at(2).lir);
    EXPECT_FALSE(c.debug_element_infos().at(4).lir);
    EXPECT_TRUE (c.debug_element_infos().at(1).lir);
    EXPECT_TRUE (c.debug_element_infos().at(3).lir);

    EXPECT_EQ(c.debug_lir_count(), c.debug_lir_capacity());

    // 4 in the queue.
    ASSERT_FALSE(c.debug_queue().empty());
    auto qit = std::find(c.debug_queue().begin(), c.debug_queue().end(), 4);
    EXPECT_NE(qit, c.debug_queue().end());


    check_lirs_structure(c, "after reinsert");
}

TEST_F(LIRSTest, ExtractHirDoesNotSetLastPromoted)
{
    init(5);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);
    c.insert(5, 50);
    // lir: [1 2 3 4], queue: [5]

    ASSERT_EQ(c.debug_lir_count(), 4u);
    ASSERT_FALSE(c.debug_element_infos().at(5).lir);

    auto v = c.extract(5);
    ASSERT_TRUE(v.has_value());

    EXPECT_EQ(c.debug_lir_count(), 4u);
    EXPECT_TRUE(c.debug_in_ghost(5));
    EXPECT_FALSE(c.debug_ghost_is_lir(5));

    check_lirs_structure(c, "after extract HIR");
}

TEST_F(LIRSTest, ReinsertAfterExtractHirDoesNotAffectLir)
{
    init(5);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);
    c.insert(5, 50);

    const auto before_lir = c.debug_lir_count();

    c.extract(5);
    c.insert(5, 51);

    EXPECT_FALSE(c.debug_element_infos().at(5).lir);
    EXPECT_TRUE (c.debug_element_infos().at(1).lir);
    EXPECT_TRUE (c.debug_element_infos().at(2).lir);
    EXPECT_TRUE (c.debug_element_infos().at(3).lir);
    EXPECT_TRUE (c.debug_element_infos().at(4).lir);

    EXPECT_EQ(c.debug_lir_count(), before_lir);

    
    check_lirs_structure(c, "after reinsert HIR");
}



// --- long sequence ---


TEST_F(LIRSTest, InvTest)
{
    constexpr std::size_t CAP       = 1024;
    constexpr int         KEY_RANGE = 4096;
    constexpr int         OPS       = 100000;

    init(CAP);
    auto& c = *cache;

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

        else if (o < 80)
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

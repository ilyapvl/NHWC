#include <gtest/gtest.h>
#include "lirs_cache.h"
#include "cache_test_invariants.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

using cache::LIRSCache;

using namespace cache_test;

class LIRSTest : public ::testing::Test
{
protected:
    std::unique_ptr<LIRSCache<int, int>> cache;

    void init(std::size_t capacity)
    {
        cache = std::make_unique<LIRSCache<int, int>>(capacity, capacity);
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

TEST_F(LIRSTest, EvictionPicksQueueBack)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40); // HIR

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



TEST_F(LIRSTest, ExtractLirPreservesLirInGhost)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 3);

    ASSERT_TRUE(c.debug_element_infos().at(2).lir);

    auto v = c.extract_ptr(2);

    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 20);
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
    c.insert(4, 40);  // HIR

    ASSERT_EQ(c.debug_lir_count(), 3u);
    ASSERT_EQ(c.debug_queue().size(), 1u);
    ASSERT_FALSE(c.debug_element_infos().at(4).lir);

    auto v = c.extract_ptr(4);

    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 40);
    EXPECT_FALSE(c.contains(4));
    EXPECT_TRUE(c.debug_in_ghost(4));
    EXPECT_FALSE(c.debug_ghost_is_lir(4));
    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, ReinsertAfterExtractLirBecomesLir)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 3);

    c.extract_ptr(2);
    ASSERT_TRUE(c.debug_ghost_is_lir(2));

    // reinsert 2 >> 4 back to hir
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
    c.insert(4, 40);  // HIR

    c.extract_ptr(4);
    ASSERT_FALSE(c.debug_ghost_is_lir(4));

    c.insert(4, 41);

    EXPECT_TRUE(c.contains(4));
    EXPECT_FALSE(c.debug_element_infos().at(4).lir);
    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, ExtractMiss)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 3);

    auto v = c.extract_ptr(999);

    EXPECT_EQ(v, nullptr);
    EXPECT_EQ(resident_count(c), 3u);
    check_lirs_structure(c, "unit");
}


TEST_F(LIRSTest, GetMissNoStateChange)
{
    init(4);
    auto& c = *cache;
    fill_seq(c, 3);

    const auto stack_before = c.debug_stack();
    const auto queue_before = c.debug_queue();
    const auto res_before   = resident_count(c);
    const auto lir_before   = c.debug_lir_count();

    auto v = c.get(999);

    EXPECT_FALSE(v.has_value());
    EXPECT_EQ(c.debug_stack(), stack_before);
    EXPECT_EQ(c.debug_queue(), queue_before);
    EXPECT_EQ(resident_count(c), res_before);
    EXPECT_EQ(c.debug_lir_count(), lir_before);
    check_lirs_structure(c, "unit");
}

TEST_F(LIRSTest, UpdateValueKeepsSize)
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
    EXPECT_EQ(*c.debug_values().at(2), 222);

    
    check_lirs_structure(c, "unit");
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

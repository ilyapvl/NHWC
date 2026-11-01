#include <gtest/gtest.h>
#include "twoq_cache.h"
#include "cache_test_invariants.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

using cache::TwoQCache;
using cache_test::check_twoq_structure;
using cache_test::twoq_tag_name;
using cache_test::TWOQ_NOT_FOUND;
using cache_test::TWOQ_Q1;
using cache_test::TWOQ_Q2;
using cache_test::TWOQ_GHOST;

class TwoQTest : public ::testing::Test
{
protected:
    std::unique_ptr<TwoQCache<int, int>> cache;

    void init(std::size_t capacity)
    {
        cache = std::make_unique<TwoQCache<int, int>>(capacity, capacity);
    }

    void TearDown() override
    {
        if (::testing::Test::HasFailure() && cache)
            cache->dump(std::cerr);
    }
};




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
    EXPECT_EQ(c.debug_list_tag(1), TWOQ_Q1);
    EXPECT_EQ(c.debug_q1().front(), 1);
    EXPECT_EQ(c.debug_q1().size(), 1u);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, InsertExistingInQ1StaysInQ1)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    ASSERT_EQ(c.debug_list_tag(1), TWOQ_Q1);

    auto ev = c.insert(1, 11);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(c.debug_q1().size(), 1u);
    EXPECT_EQ(c.debug_list_tag(1), TWOQ_Q1);
    EXPECT_EQ(*c.get(1), 11);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, InsertExistingInQ2StaysInQ2)
{
    init(4);
    auto& c = *cache;

    // q1=[5, 4, 3], q2=[1], ghost=[2]
    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);
    c.insert(5, 50);
    c.insert(1, 100);

    ASSERT_EQ(c.debug_list_tag(1), TWOQ_Q2);

    auto ev = c.insert(1, 111);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(c.debug_list_tag(1), TWOQ_Q2);
    EXPECT_EQ(c.debug_q2().front(), 1);
    EXPECT_EQ(*c.get(1), 111);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, GetQ1HitStaysInQ1)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    ASSERT_EQ(c.debug_list_tag(1), TWOQ_Q1);

    auto v = c.get(1);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 10);
    EXPECT_EQ(c.debug_list_tag(1), TWOQ_Q1);
    EXPECT_EQ(c.debug_q1().size(), 2u);
    EXPECT_EQ(c.debug_q2().size(), 0u);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, GetQ2HitMovesToFront)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);
    c.insert(5, 50);      // q1=[5,4,3,2], ghost=[1]
    c.insert(1, 100);     // q1=[5,4,3], q2=[1], ghost=[2]
    c.insert(2, 200);     // q1=[5,4], q2=[2,1], ghost=[3]

    ASSERT_EQ(c.debug_list_tag(1), TWOQ_Q2);
    ASSERT_EQ(c.debug_list_tag(2), TWOQ_Q2);
    ASSERT_EQ(c.debug_q2().front(), 2);
    ASSERT_EQ(c.debug_q2().back(), 1);

    auto v = c.get(1);

    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 100);
    EXPECT_EQ(c.debug_q2().front(), 1);
    EXPECT_EQ(c.debug_q2().back(), 2);

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

TEST_F(TwoQTest, ExtractQ1SavesAsQ1)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    ASSERT_EQ(c.debug_list_tag(1), TWOQ_Q1);

    auto v = c.extract_ptr(1);

    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 10);
    EXPECT_FALSE(c.contains(1));
    EXPECT_TRUE(c.debug_in_ghost(1));
    EXPECT_FALSE(c.debug_ghost_is_q2(1));
    EXPECT_EQ(c.debug_q1().size(), 0u);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, ExtractQ2SavesAsQ2)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);
    c.insert(5, 50);
    c.insert(1, 100); // q2

    ASSERT_EQ(c.debug_list_tag(1), TWOQ_Q2);

    auto v = c.extract_ptr(1);

    ASSERT_NE(v, nullptr);
    EXPECT_FALSE(c.contains(1));
    EXPECT_TRUE(c.debug_in_ghost(1));
    EXPECT_TRUE(c.debug_ghost_is_q2(1));
    EXPECT_EQ(c.debug_q2().size(), 0u);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, ReinsertAfterExtractQ1ReturnsToQ1)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);
    ASSERT_EQ(c.debug_list_tag(1), TWOQ_Q1);

    c.extract_ptr(1);
    ASSERT_TRUE(c.debug_in_ghost(1));
    ASSERT_FALSE(c.debug_ghost_is_q2(1));

    c.insert(1, 11);

    EXPECT_EQ(c.debug_list_tag(1), TWOQ_Q1);
    EXPECT_EQ(c.debug_q1().front(), 1);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, ReinsertAfterExtractQ2ReturnsToQ2)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);
    c.insert(4, 40);
    c.insert(5, 50);
    c.insert(1, 100); // q2

    ASSERT_EQ(c.debug_list_tag(1), TWOQ_Q2);

    c.extract_ptr(1);
    ASSERT_TRUE(c.debug_in_ghost(1));
    ASSERT_TRUE(c.debug_ghost_is_q2(1));

    c.insert(1, 111);

    EXPECT_EQ(c.debug_list_tag(1), TWOQ_Q2);
    EXPECT_EQ(c.debug_q2().front(), 1);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, ListGhostHitPromotesToQ2)
{
    init(8);
    auto& c = *cache;

    insert_n(c, 9);

    ASSERT_EQ(c.debug_list_tag(1), TWOQ_GHOST);
    ASSERT_FALSE(c.debug_q1().empty());
    const int q1_back = c.debug_q1().back();

    auto ev = c.insert(1, 100);

    ASSERT_TRUE(ev.has_value());
    EXPECT_EQ(ev->first, q1_back);
    EXPECT_EQ(c.debug_list_tag(1), TWOQ_Q2);
    EXPECT_EQ(c.debug_q2().front(), 1);

    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, Q1OverflowPushesToGhost)
{
    init(8);
    auto& c = *cache;

    insert_n(c, 9);

    EXPECT_EQ(c.debug_list_tag(1), TWOQ_GHOST);
    EXPECT_EQ(c.debug_ghost().size(), 1u);
    EXPECT_EQ(c.debug_ghost().front(), 1);

    EXPECT_EQ(c.debug_list_tag(2), TWOQ_Q1);
    EXPECT_EQ(c.debug_q1().size(), 8u);

    check_twoq_structure(c, "unit");
}



TEST_F(TwoQTest, GhostTrimRespectsTargetSize)
{
    init(8);
    auto& c = *cache;

    insert_n(c, 20);

    EXPECT_EQ(c.debug_ghost().size(), 4u);
    EXPECT_EQ(c.debug_list_tag(12), TWOQ_GHOST);
    EXPECT_EQ(c.debug_list_tag(11), TWOQ_GHOST);
    EXPECT_EQ(c.debug_list_tag(10), TWOQ_GHOST);
    EXPECT_EQ(c.debug_list_tag(9),  TWOQ_GHOST);

    EXPECT_EQ(c.debug_list_tag(1), TWOQ_NOT_FOUND);
    EXPECT_EQ(c.debug_list_tag(2), TWOQ_NOT_FOUND);

    check_twoq_structure(c, "unit");
}


TEST_F(TwoQTest, ExtractMiss)
{
    init(8);
    auto& c = *cache;

    c.insert(1, 10);

    auto v = c.extract_ptr(999);

    EXPECT_EQ(v, nullptr);
    EXPECT_EQ(c.debug_q1().size(), 1u);
    check_twoq_structure(c, "unit");
}


TEST_F(TwoQTest, GetMissNoStateChange)
{
    init(8);
    auto& c = *cache;
    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);

    const auto q1_before    = c.debug_q1();
    const auto q2_before    = c.debug_q2();
    const auto ghost_before = c.debug_ghost();

    auto v = c.get(999);

    EXPECT_FALSE(v.has_value());
    EXPECT_EQ(c.debug_q1(), q1_before);
    EXPECT_EQ(c.debug_q2(), q2_before);
    EXPECT_EQ(c.debug_ghost(), ghost_before);
    check_twoq_structure(c, "unit");
}

TEST_F(TwoQTest, UpdateValueKeepsSize)
{
    init(8);
    auto& c = *cache;
    c.insert(1, 10);
    c.insert(2, 20);

    const auto before_res = c.debug_q1().size() + c.debug_q2().size();

    auto ev = c.insert(1, 111);  // Q1

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(c.debug_q1().size() + c.debug_q2().size(), before_res);
    EXPECT_EQ(c.debug_list_tag(1), TWOQ_Q1);
    EXPECT_EQ(*c.debug_values().at(1), 111);
    check_twoq_structure(c, "unit");
}


int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

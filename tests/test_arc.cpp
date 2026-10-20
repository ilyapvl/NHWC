#include <gtest/gtest.h>
#include "arc_cache.h"
#include "cache_test_invariants.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>



using cache::ARCCache;

using namespace cache_test;

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
        cache = std::make_unique<ARCCache<int, int>>(capacity, capacity);
    }

    void TearDown() override
    {
        if (::testing::Test::HasFailure() && cache)
            cache->dump(std::cerr);
    }
};

// --- unit ---


TEST_F(ARCTest, InsertIntoEmptyInT1)
{
    init(4);
    auto& c = *cache;

    auto ev = c.insert(1, 10);

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(c.debug_list_tag(1), ARC_T1);
    EXPECT_EQ(c.debug_t1().front(), 1);
    EXPECT_EQ(c.debug_target_t1_size(), 0.0);

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
    ASSERT_EQ(c.debug_target_t1_size(), 0.0);

    // p increases by 1, key moves to T2
    // T1.back() evicted to B1
    auto ev2 = c.insert(2, 22);

    ASSERT_TRUE(ev2.has_value());
    EXPECT_EQ(ev2->first, 3);   

    EXPECT_EQ(c.debug_target_t1_size(), 1.0) << "p must increase on B1 hit";
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
    ASSERT_EQ(c.debug_target_t1_size(), 1.0);

    // B2 hit so p = 0, replace evicts T1.back() to B1
    auto ev3 = c.insert(3, 33);

    ASSERT_TRUE(ev3.has_value());
    EXPECT_EQ(ev3->first, 5);

    EXPECT_EQ(c.debug_target_t1_size(), 0.0) << "p must decrease on B2 hit";
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



TEST_F(ARCTest, ExtractT1SavesAsT1)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    ASSERT_EQ(c.debug_list_tag(1), ARC_T1);

    auto v = c.extract_ptr(1);

    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 10);
    EXPECT_FALSE(c.contains(1));
    EXPECT_TRUE(c.debug_in_ghost(1));
    EXPECT_FALSE(c.debug_ghost_is_t2(1));
    EXPECT_EQ(c.debug_t1().size(), 0u);
    check_arc_structure(c, "unit");
}

TEST_F(ARCTest, ExtractT2SavesAsT2)
{
    init(4);
    auto& c = *cache;

    c.insert(1, 10);
    c.get(1);  // move to T2
    ASSERT_EQ(c.debug_list_tag(1), ARC_T2);

    auto v = c.extract_ptr(1);

    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 10);
    EXPECT_FALSE(c.contains(1));
    EXPECT_TRUE(c.debug_in_ghost(1));
    EXPECT_TRUE(c.debug_ghost_is_t2(1));
    EXPECT_EQ(c.debug_t2().size(), 0u);
    check_arc_structure(c, "unit");
}

TEST_F(ARCTest, ReinsertAfterExtractT1ReturnsToT1)
{
    init(2);
    auto& c = *cache;

    c.insert(1, 10);
    c.insert(2, 20);
    ASSERT_EQ(c.debug_list_tag(1), ARC_T1);

    c.extract_ptr(1);
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

    c.extract_ptr(1);
    ASSERT_TRUE(c.debug_in_ghost(1));
    ASSERT_TRUE(c.debug_ghost_is_t2(1));

    c.insert(1, 11);

    EXPECT_EQ(c.debug_list_tag(1), ARC_T2);
    EXPECT_EQ(c.debug_t2().front(), 1);
    check_arc_structure(c, "unit");
}

TEST_F(ARCTest, ExtractMiss)
{
    init(4);
    auto& c = *cache;
    c.insert(1, 10);

    auto v = c.extract_ptr(999);

    EXPECT_EQ(v, nullptr);
    EXPECT_EQ(c.debug_t1().size(), 1u);
    check_arc_structure(c, "unit");
}


TEST_F(ARCTest, GetMissNoStateChange)
{
    init(4);
    auto& c = *cache;
    c.insert(1, 10);
    c.insert(2, 20);
    c.insert(3, 30);

    const auto t1_before = c.debug_t1();
    const auto t2_before = c.debug_t2();
    const auto b1_before = c.debug_b1();
    const auto b2_before = c.debug_b2();
    const auto p_before  = c.debug_target_t1_size();

    auto v = c.get(999);

    EXPECT_FALSE(v.has_value());
    EXPECT_EQ(c.debug_t1(), t1_before);
    EXPECT_EQ(c.debug_t2(), t2_before);
    EXPECT_EQ(c.debug_b1(), b1_before);
    EXPECT_EQ(c.debug_b2(), b2_before);
    EXPECT_EQ(c.debug_target_t1_size(), p_before);
    check_arc_structure(c, "unit");
}

TEST_F(ARCTest, UpdateValueKeepsSize)
{
    init(4);
    auto& c = *cache;
    c.insert(1, 10);
    c.insert(2, 20);

    const auto before_res = c.debug_t1().size() + c.debug_t2().size();
    const auto before_p   = c.debug_target_t1_size();

    auto ev = c.insert(1, 111);  // to T2

    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(c.debug_t1().size() + c.debug_t2().size(), before_res);
    EXPECT_EQ(c.debug_target_t1_size(), before_p);
    EXPECT_EQ(c.debug_list_tag(1), ARC_T2);
    EXPECT_EQ(*c.debug_values().at(1), 111);
    check_arc_structure(c, "unit");
}


int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

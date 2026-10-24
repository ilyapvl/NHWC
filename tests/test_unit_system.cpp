#include <gtest/gtest.h>
#include "cache_system.h"
#include "cache_system_invariants.h"

#include <functional>
#include <memory>
#include <stdexcept>

using namespace cache;
using namespace cache_test;

class CacheSystemTest : public ::testing::Test
{
protected:
    int slow_calls = 0;
    std::unique_ptr<CacheSystem<int, int>> cs;

    void init()
    {
        cs = std::make_unique<CacheSystem<int, int>>(
            [this](const int& k) { return slow_get(k); });
    }

    int slow_get(int key)
    {
        slow_calls++;
        return key * 100;
    }
};


static bool level_contains(CacheSystem<int,int>& cs, std::size_t lvl, int key)
{
    return cs.levels()[lvl]->debug_values().count(key) > 0;
}

static int level_value(CacheSystem<int,int>& cs, std::size_t lvl, int key)
{
    const auto& vals = cs.levels()[lvl]->debug_values();
    auto it = vals.find(key);
    return (it != vals.end()) ? *it->second : -1;
}






TEST_F(CacheSystemTest, ConstructsEmpty)
{
    init();

    EXPECT_EQ(cs->levels().size(), 0u);
    EXPECT_EQ(slow_calls, 0);
    EXPECT_EQ(cs->get_last_hit_level(), 0);

    check_system_structure(*cs, "unit");
}


TEST_F(CacheSystemTest, AddEachKnownAlgorithmSucceeds)
{
    init();

    EXPECT_EQ(cs->add_cache(CacheAlgorithm::LRU,  4), NO_ERR);
    EXPECT_EQ(cs->add_cache(CacheAlgorithm::LFU,  4), NO_ERR);
    EXPECT_EQ(cs->add_cache(CacheAlgorithm::LIRS, 4), NO_ERR);
    EXPECT_EQ(cs->add_cache(CacheAlgorithm::ARC,  4), NO_ERR);
    EXPECT_EQ(cs->add_cache(CacheAlgorithm::TwoQ,   4), NO_ERR);

    EXPECT_EQ(cs->levels().size(), 5u);

    check_system_structure(*cs, "unit");
}


TEST_F(CacheSystemTest, AddUnknownAlgorithmReturnsInvalidAlgorithm)
{
    init();

    EXPECT_EQ(cs->add_cache(algorithm_from_string("abcd"), 4), INVALID_ALGORITHM);
    EXPECT_EQ(cs->levels().size(), 0u);

    check_system_structure(*cs, "unit");
}

TEST_F(CacheSystemTest, AddLruZeroCapacityAllowed)
{
    init();

    EXPECT_EQ(cs->add_cache(CacheAlgorithm::LRU, 0), NO_ERR);
    EXPECT_EQ(cs->levels().size(), 1u);

    check_system_structure(*cs, "unit");
}

TEST_F(CacheSystemTest, AddLfuZeroCapacityRejected)
{
    init();

    EXPECT_EQ(cs->add_cache(CacheAlgorithm::LFU, 0), INVALID_CAPACITY);
    EXPECT_EQ(cs->levels().size(), 0u);

    check_system_structure(*cs, "unit");
}

TEST_F(CacheSystemTest, AddLirsZeroCapacityRejected)
{
    init();

    EXPECT_EQ(cs->add_cache(CacheAlgorithm::LIRS, 0), INVALID_CAPACITY);
    EXPECT_EQ(cs->levels().size(), 0u);

    check_system_structure(*cs, "unit");
}

TEST_F(CacheSystemTest, AddArcZeroCapacityRejected)
{
    init();

    EXPECT_EQ(cs->add_cache(CacheAlgorithm::ARC, 0), INVALID_CAPACITY);
    EXPECT_EQ(cs->levels().size(), 0u);

    check_system_structure(*cs, "unit");
}

TEST_F(CacheSystemTest, AddTwoQOneCapacityRejected)
{
    init();

    EXPECT_EQ(cs->add_cache(CacheAlgorithm::TwoQ, 1), INVALID_CAPACITY);
    EXPECT_EQ(cs->levels().size(), 0u);

    check_system_structure(*cs, "unit");
}

TEST_F(CacheSystemTest, AddTwoQTwoCapacityAllowed)
{
    init();

    EXPECT_EQ(cs->add_cache(CacheAlgorithm::TwoQ, 2), NO_ERR);
    EXPECT_EQ(cs->levels().size(), 1u);

    check_system_structure(*cs, "unit");
}


TEST_F(CacheSystemTest, HitsVectorHasExactlyOneEntryPerLevel)
{
    init();

    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 4), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 4), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 4), NO_ERR);

    EXPECT_EQ(cs->levels().size(), 3u);

    EXPECT_NO_THROW(cs->get_hits(0));
    EXPECT_NO_THROW(cs->get_hits(1));
    EXPECT_NO_THROW(cs->get_hits(2));


    EXPECT_THROW(cs->get_hits(3), std::out_of_range);

    check_system_structure(*cs, "unit");
}





TEST_F(CacheSystemTest, MissCallsSlowGetExactlyOnce)
{
    init();
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 4), NO_ERR);

    const int v = cs->access(7);

    EXPECT_EQ(v, 700);
    EXPECT_EQ(slow_calls, 1);

    check_system_structure(*cs, "unit");
}


TEST_F(CacheSystemTest, RepeatedAccessSameKeyCallsSlowOnce)
{
    init();
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 4), NO_ERR);

    cs->access(7);
    cs->access(7);
    cs->access(7);

    EXPECT_EQ(slow_calls, 1);

    check_system_structure(*cs, "unit");
}



TEST_F(CacheSystemTest, PromotionCascades)
{
    init();
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LFU, 1), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LFU, 1), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LFU, 2), NO_ERR);

    cs->access(1);
    cs->access(2);
    cs->access(3);
    cs->access(4);  // L0=[4], L1=[3], L2=[2,1]

    cs->access(1);

    EXPECT_EQ(cs->get_last_hit_level(), 2);
    EXPECT_EQ(cs->get_hits(2), 1);

    // [1] [4] [3 2]
    EXPECT_TRUE (level_contains(*cs, 0, 1));
    EXPECT_TRUE (level_contains(*cs, 1, 4));
    EXPECT_TRUE (level_contains(*cs, 2, 3));
    EXPECT_TRUE (level_contains(*cs, 2, 2));

    EXPECT_EQ(level_value(*cs, 0, 1), 100);
    EXPECT_EQ(level_value(*cs, 1, 4), 400);
    EXPECT_EQ(level_value(*cs, 2, 3), 300);
    EXPECT_EQ(level_value(*cs, 2, 2), 200);
}

TEST_F(CacheSystemTest, HitAtLevelNCountsOnLevelN)
{
    init();
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 1), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 1), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 1), NO_ERR);

    cs->access(1);
    cs->access(2);
    cs->access(3);

    ASSERT_EQ(cs->get_hits(0), 0);
    ASSERT_EQ(cs->get_hits(1), 0);
    ASSERT_EQ(cs->get_hits(2), 0);

    cs->access(3);

    EXPECT_EQ(cs->get_hits(0), 1);
    EXPECT_EQ(cs->get_hits(1), 0);
    EXPECT_EQ(cs->get_hits(2), 0);
    EXPECT_EQ(cs->get_last_hit_level(), 0);

    cs->access(10);
    cs->access(20);
    cs->access(30);

    cs->access(20);

    EXPECT_EQ(cs->get_hits(0), 1);
    EXPECT_EQ(cs->get_hits(1), 1);
    EXPECT_EQ(cs->get_hits(2), 0);
    EXPECT_EQ(cs->get_last_hit_level(), 1);

    cs->access(100);
    cs->access(200);
    cs->access(300);

    cs->access(100);

    EXPECT_EQ(cs->get_hits(0), 1);
    EXPECT_EQ(cs->get_hits(1), 1);
    EXPECT_EQ(cs->get_hits(2), 1);
    EXPECT_EQ(cs->get_last_hit_level(), 2);

    check_system_structure(*cs, "unit");
}

TEST_F(CacheSystemTest, LastHitLevelUpdatesAfterEachHit)
{
    init();
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 1), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 1), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 1), NO_ERR);

    cs->access(1);
    cs->access(2);
    cs->access(3);

    // 1  L2, 2  L1, 3  L0
    cs->access(3);
    EXPECT_EQ(cs->get_last_hit_level(), 0);

    cs->access(2);
    EXPECT_EQ(cs->get_last_hit_level(), 1);

    cs->access(1);
    EXPECT_EQ(cs->get_last_hit_level(), 2);

    check_system_structure(*cs, "unit");
}

TEST_F(CacheSystemTest, MissDoesNotChangeLastHitLevel)
{
    init();
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 1), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 4), NO_ERR);

    cs->access(1);
    cs->access(2);
    cs->access(1); // hit at L1, last_hit_level = 1

    ASSERT_EQ(cs->get_last_hit_level(), 1);

    cs->access(999);
    EXPECT_EQ(cs->get_last_hit_level(), 1) << "miss must not change last_hit_level";

    check_system_structure(*cs, "unit");
}

TEST_F(CacheSystemTest, LastLevelEvictionDrops)
{
    init();
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 1), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 1), NO_ERR);

    cs->access(1); // L0=[1]
    cs->access(2); // L0=[2], L1=[1]
    cs->access(3); // L0=[3], L1=[2]

    EXPECT_FALSE(level_contains(*cs, 0, 1));
    EXPECT_FALSE(level_contains(*cs, 1, 1));

    const int before = slow_calls;
    cs->access(1);
    EXPECT_EQ(slow_calls, before + 1);

    EXPECT_TRUE(level_contains(*cs, 0, 1));

    check_system_structure(*cs, "unit");
}

TEST_F(CacheSystemTest, PromotionPreservesValue)
{
    init();
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 1), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 3), NO_ERR);

    cs->access(1); // slow_get_page(1)=100
    cs->access(7); // to L1

    const int before = slow_calls;
    const int v = cs->access(1);

    EXPECT_EQ(v, 100);
    EXPECT_EQ(slow_calls, before);
    EXPECT_EQ(level_value(*cs, 0, 1), 100);

    check_system_structure(*cs, "unit");
}

TEST_F(CacheSystemTest, HitAtLevel0DoesNotCascade)
{
    init();
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 2), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 2), NO_ERR);

    cs->access(1);
    cs->access(2); // L0=[2 1]

    cs->access(1); // hit L0

    EXPECT_TRUE (level_contains(*cs, 0, 1));
    EXPECT_TRUE (level_contains(*cs, 0, 2));
    EXPECT_FALSE(level_contains(*cs, 1, 1));
    EXPECT_FALSE(level_contains(*cs, 1, 2));
    EXPECT_EQ(cs->get_last_hit_level(), 0);
    EXPECT_EQ(cs->get_hits(0), 1);
    EXPECT_EQ(cs->get_hits(1), 0);

    check_system_structure(*cs, "unit");
}


TEST_F(CacheSystemTest, ZeroCapacity)
{
    init();
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 0), NO_ERR);
    ASSERT_EQ(cs->add_cache(CacheAlgorithm::LRU, 3), NO_ERR);

    cs->access(1);
    EXPECT_FALSE(level_contains(*cs, 0, 1));
    EXPECT_TRUE (level_contains(*cs, 1, 1));

    check_system_structure(*cs, "unit");
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

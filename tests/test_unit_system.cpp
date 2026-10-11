#include <gtest/gtest.h>
#include "cache_system.h"

#include <functional>
#include <memory>
#include <stdexcept>

using namespace cache;

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




TEST_F(CacheSystemTest, ConstructsEmpty)
{
    init();

    EXPECT_EQ(cs->levels().size(), 0u);
    EXPECT_EQ(slow_calls, 0);
    EXPECT_EQ(cs->get_last_hit_level(), 0);
}


TEST_F(CacheSystemTest, AddEachKnownAlgorithmSucceeds)
{
    init();

    EXPECT_EQ(cs->add_cache("LRU",  4), NO_ERR);
    EXPECT_EQ(cs->add_cache("LFU",  4), NO_ERR);
    EXPECT_EQ(cs->add_cache("LIRS", 4), NO_ERR);
    EXPECT_EQ(cs->add_cache("ARC",  4), NO_ERR);
    EXPECT_EQ(cs->add_cache("2Q",   4), NO_ERR);

    EXPECT_EQ(cs->levels().size(), 5u);
}


TEST_F(CacheSystemTest, AddUnknownAlgorithmReturnsInvalidAlgorithm)
{
    init();

    EXPECT_EQ(cs->add_cache("abcd", 4), INVALID_ALGORITHM);
    EXPECT_EQ(cs->levels().size(), 0u);
}

TEST_F(CacheSystemTest, AddLruZeroCapacityAllowed)
{
    init();

    EXPECT_EQ(cs->add_cache("LRU", 0), NO_ERR);
    EXPECT_EQ(cs->levels().size(), 1u);
}

TEST_F(CacheSystemTest, AddLfuZeroCapacityRejected)
{
    init();

    EXPECT_EQ(cs->add_cache("LFU", 0), INVALID_CAPACITY);
    EXPECT_EQ(cs->levels().size(), 0u);
}

TEST_F(CacheSystemTest, AddLirsZeroCapacityRejected)
{
    init();

    EXPECT_EQ(cs->add_cache("LIRS", 0), INVALID_CAPACITY);
    EXPECT_EQ(cs->levels().size(), 0u);
}

TEST_F(CacheSystemTest, AddArcZeroCapacityRejected)
{
    init();

    EXPECT_EQ(cs->add_cache("ARC", 0), INVALID_CAPACITY);
    EXPECT_EQ(cs->levels().size(), 0u);
}

TEST_F(CacheSystemTest, AddTwoQOneCapacityRejected)
{
    init();

    EXPECT_EQ(cs->add_cache("2Q", 1), INVALID_CAPACITY);
    EXPECT_EQ(cs->levels().size(), 0u);
}

TEST_F(CacheSystemTest, AddTwoQTwoCapacityAllowed)
{
    init();

    EXPECT_EQ(cs->add_cache("2Q", 2), NO_ERR);
    EXPECT_EQ(cs->levels().size(), 1u);
}


TEST_F(CacheSystemTest, HitsVectorHasExactlyOneEntryPerLevel)
{
    init();

    ASSERT_EQ(cs->add_cache("LRU", 4), NO_ERR);
    ASSERT_EQ(cs->add_cache("LRU", 4), NO_ERR);
    ASSERT_EQ(cs->add_cache("LRU", 4), NO_ERR);

    EXPECT_EQ(cs->levels().size(), 3u);

    EXPECT_NO_THROW(cs->get_hits(0));
    EXPECT_NO_THROW(cs->get_hits(1));
    EXPECT_NO_THROW(cs->get_hits(2));


    EXPECT_THROW(cs->get_hits(3), std::out_of_range);
}





TEST_F(CacheSystemTest, MissCallsSlowGetExactlyOnce)
{
    init();
    ASSERT_EQ(cs->add_cache("LRU", 4), NO_ERR);

    const int v = cs->access(7);

    EXPECT_EQ(v, 700);
    EXPECT_EQ(slow_calls, 1);
}


TEST_F(CacheSystemTest, RepeatedAccessSameKeyCallsSlowOnce)
{
    init();
    ASSERT_EQ(cs->add_cache("LRU", 4), NO_ERR);

    cs->access(7);
    cs->access(7);
    cs->access(7);

    EXPECT_EQ(slow_calls, 1);
}


TEST_F(CacheSystemTest, HitAtLevelZeroCountsOnLevelZero)
{
    init();
    ASSERT_EQ(cs->add_cache("LRU", 4), NO_ERR);
    ASSERT_EQ(cs->add_cache("LRU", 4), NO_ERR);

    cs->access(1);
    cs->access(1);

    EXPECT_EQ(cs->get_hits(0), 1);
    EXPECT_EQ(cs->get_hits(1), 0);
    EXPECT_EQ(cs->get_last_hit_level(), 0);
}

TEST_F(CacheSystemTest, HitAtLevelOneCountsOnLevelOne)
{
    init();
    ASSERT_EQ(cs->add_cache("LRU", 1), NO_ERR);
    ASSERT_EQ(cs->add_cache("LRU", 4), NO_ERR);

    cs->access(1);
    cs->access(2);

    ASSERT_EQ(cs->get_hits(0), 0);
    ASSERT_EQ(cs->get_hits(1), 0);

    cs->access(1);

    EXPECT_EQ(cs->get_hits(0), 0);
    EXPECT_EQ(cs->get_hits(1), 1);
    EXPECT_EQ(cs->get_last_hit_level(), 1);
}

TEST_F(CacheSystemTest, LastHitLevelUpdatesAfterEachHit)
{
    init();
    ASSERT_EQ(cs->add_cache("LRU", 1), NO_ERR);
    ASSERT_EQ(cs->add_cache("LRU", 1), NO_ERR);
    ASSERT_EQ(cs->add_cache("LRU", 1), NO_ERR);

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
}

TEST_F(CacheSystemTest, MissDoesNotChangeLastHitLevel)
{
    init();
    ASSERT_EQ(cs->add_cache("LRU", 1), NO_ERR);
    ASSERT_EQ(cs->add_cache("LRU", 4), NO_ERR);

    cs->access(1);
    cs->access(2);
    cs->access(1); // hit at L1, last_hit_level = 1

    ASSERT_EQ(cs->get_last_hit_level(), 1);

    cs->access(999);
    EXPECT_EQ(cs->get_last_hit_level(), 1) << "miss must not change last_hit_level";
}




int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

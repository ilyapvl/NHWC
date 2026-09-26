#include <gtest/gtest.h>
#include "opt_cache.h"

#include <vector>

TEST(OptimalCacheUnit, EmptySequence)
{
    std::vector<int> seq{};
    OptimalCache<int, int> cache(3, seq);
    EXPECT_EQ(cache.simulate(), 0);
}

TEST(OptimalCacheUnit, ZeroCapacity)
{
    std::vector<int> seq{1, 2, 3, 1, 2, 3};
    OptimalCache<int, int> cache(0, seq);
    EXPECT_EQ(cache.simulate(), 0);
}

TEST(OptimalCacheUnit, CapacityOne)
{
    std::vector<int> seq{1, 2, 3, 1, 2, 3};
    OptimalCache<int, int> cache(1, seq);

    EXPECT_EQ(cache.simulate(), 0);
}

TEST(OptimalCacheUnit, CapacityOneWithRepeats)
{
    std::vector<int> seq{1, 1, 1, 1};
    OptimalCache<int, int> cache(1, seq);
    EXPECT_EQ(cache.simulate(), 3);
}


TEST(OptimalCacheUnit, BeladyClassic)
{
    std::vector<int> seq{7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    OptimalCache<int, int> cache(3, seq);
    EXPECT_EQ(cache.simulate(), 11);
}

TEST(OptimalCacheUnit, Three)
{
    std::vector<int> seq{1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3};
    OptimalCache<int, int> cache(2, seq);
    EXPECT_EQ(cache.simulate(), 5);
}

TEST(OptimalCacheUnit, AllSameKey)
{
    std::vector<int> seq{1, 1, 1, 1, 1};
    OptimalCache<int, int> cache(1, seq);
    EXPECT_EQ(cache.simulate(), 4);
}

TEST(OptimalCacheUnit, doesNotEvictHot)
{
    std::vector<int> seq{1, 2, 3, 4, 5, 6, 1};
    OptimalCache<int, int> cache(2, seq);

    EXPECT_EQ(cache.simulate(), 1);
}

TEST(OptimalCacheUnit, Hotpair)
{
    std::vector<int> seq{1, 2, 3, 4, 5, 6, 1, 2};
    OptimalCache<int, int> cache(2, seq);

    EXPECT_EQ(cache.simulate(), 1);
}


TEST(OptimalCacheUnit, Seq)
{
    std::vector<int> seq{1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4};
    OptimalCache<int, int> cache(3, seq);

    EXPECT_EQ(cache.simulate(), 6);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

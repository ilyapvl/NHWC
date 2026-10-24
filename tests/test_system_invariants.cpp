#include <gtest/gtest.h>
#include "cache_system.h"
#include "cache_system_invariants.h"

#include <cctype>
#include <cstdint>
#include <memory>
#include <ostream>
#include <random>
#include <string>
#include <vector>

using cache::CacheSystem;
using CacheAlgorithm::LRU;
using CacheAlgorithm::LFU;
using CacheAlgorithm::LIRS;
using CacheAlgorithm::ARC;
using CacheAlgorithm::TwoQ;

struct SystemConfig
{
    std::string name;
    std::vector<std::pair<CacheAlgorithm, std::size_t>> levels;
    int n_ops;
    int key_range;
    std::uint32_t seed;
};

void PrintTo(const SystemConfig& c, std::ostream* os)
{
    *os << c.name
        << " ops=" << c.n_ops
        << " key_range=" << c.key_range
        << " seed=" << c.seed;
}

class SystemInvariantTest : public ::testing::TestWithParam<SystemConfig>
{
protected:
    int slow_calls = 0;
    std::unique_ptr<CacheSystem<int,int>> cs;

    void SetUp() override
    {
        cs = std::make_unique<CacheSystem<int,int>>([this](const int& k) { return k * 100; });

        for (const auto& [algo, cap] : GetParam().levels) ASSERT_EQ(cs->add_cache(algo, cap), cache::NO_ERR);

        ASSERT_EQ(cs->levels().size(), GetParam().levels.size());
    }
};


TEST_P(SystemInvariantTest, RandomAccess) // slow_get_page = key * 100
{
    const auto& p = GetParam();
    std::mt19937 rng(p.seed);
    std::uniform_int_distribution<int> dist(0, p.key_range - 1);

    const std::size_t L = p.levels.size();
    ASSERT_GT(L, 0u);

    for (int step = 0; step < p.n_ops; step++)
    {
        const int key = dist(rng);

        SCOPED_TRACE("step " + std::to_string(step) +" key=" + std::to_string(key));

        const int v = cs->access(key);
        EXPECT_EQ(v, key * 100);

        cache_test::check_system_structure(*cs, "step=" + std::to_string(step) + " key=" + std::to_string(key));

        if (::testing::Test::HasFailure()) return;
    }

    cache_test::check_system_structure(*cs, "final");
}


std::vector<SystemConfig> make_configs()
{
    std::vector<SystemConfig> c;

    // 1 level
    c.push_back({"LRU_single",  {{LRU, 50}},  10000, 500, 1});
    c.push_back({"LFU_single",  {{LFU, 50}},  10000, 500, 2});
    c.push_back({"LIRS_single", {{LIRS, 50}}, 10000, 500, 3});
    c.push_back({"ARC_single",  {{ARC, 50}},  10000, 500, 4});
    c.push_back({"2Q_single",   {{TwoQ, 50}}, 10000, 500, 5});

    // 2 levels mixed
    c.push_back({"LRU_LFU",  {{LRU, 25},  {LFU, 100}},  50000, 1000, 20});
    c.push_back({"LFU_LRU",  {{LFU, 25},  {LRU, 100}},  50000, 1000, 21});

    c.push_back({"LRU_LIRS",  {{LRU, 25},  {LIRS, 100}},  50000, 1000, 22});
    c.push_back({"LIRS_LRU",  {{LIRS, 25},  {LRU, 100}},  50000, 1000, 23});

    c.push_back({"LRU_ARC",  {{LRU, 25},  {ARC, 100}},  50000, 1000, 22});
    c.push_back({"ARC_LRU",  {{ARC, 25},  {LRU, 100}},  50000, 1000, 23});

    c.push_back({"LRU_2Q",   {{LRU, 25},  {TwoQ, 100}},   50000, 1000, 24});
    c.push_back({"2Q_LRU",   {{TwoQ, 25},   {LRU, 100}},  50000, 1000, 25});

    // 3 leves mixed
    c.push_back({"LRU_LFU_ARC", {{LRU, 10}, {LFU, 25}, {ARC, 100}},  500000, 1000, 30});
    c.push_back({"ARC_LIRS_2Q", {{ARC, 10}, {LIRS, 25}, {TwoQ, 100}},  50000, 1000, 31});

    // all
    c.push_back({"all_five", {{LRU, 25}, {LFU, 50}, {ARC, 100}, {TwoQ, 200}, {LIRS, 500}}, 50000, 5000, 40});

    return c;
}

INSTANTIATE_TEST_SUITE_P(
    SystemConfigs,
    SystemInvariantTest,
    ::testing::ValuesIn(make_configs()),
    [](const ::testing::TestParamInfo<SystemConfig>& info) {
        return info.param.name;
    });


int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

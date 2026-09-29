#include "runner.h"
#include "cache_system.h"

#include <chrono>
#include <numeric>

std::size_t prefix_cost(const AccessCosts& c, std::size_t upto_level)
{
    std::size_t s = 0;
    for (std::size_t i = 0; i <= upto_level && i < c.level_ns.size(); i++)
    {
        s += c.level_ns[i];
    }
    
    return s;
}

std::size_t full_check_cost(const AccessCosts& c)
{
    return std::accumulate(c.level_ns.begin(), c.level_ns.end(), std::size_t{0});
}

bool run(const std::vector<int>& sequence, const std::vector<LevelSpec>& levels, const AccessCosts& costs, RunResult& r)
{
    CacheSystem<int, int> cs(levels.size(), [](const int& key) { return key; });
    for (const auto& lvl : levels)
    {
        cs.add_cache(lvl.algorithm, lvl.capacity);
    }

    
    for (int key : sequence) cs.access(key);


    r.hits_by_level.assign(levels.size(), 0);
    for (std::size_t i = 0; i < levels.size(); ++i)
    {
        r.hits_by_level[i] = cs.get_hits(static_cast<int>(i));
        r.hits += r.hits_by_level[i];
    }
    r.misses = sequence.size() - r.hits;

    std::size_t sim_ns = 0;
    for (std::size_t i = 0; i < r.hits_by_level.size(); ++i)
    {
        sim_ns += r.hits_by_level[i] * prefix_cost(costs, i);
    }

    sim_ns += r.misses * (full_check_cost(costs) + costs.miss_ns);
    sim_ns += sequence.size() * costs.overhead_ns;

    r.sim_ns = sim_ns;
    r.sim_ns_per_op = sequence.empty() ? 0.0 : static_cast<double>(sim_ns) / sequence.size();


    return true;
}

#ifndef RUNNER_H
#define RUNNER_H

#include <cstddef>
#include <string>
#include <vector>

struct LevelSpec
{
    std::string algorithm;
    std::size_t capacity = 0;
};

struct AccessCosts
{
    std::vector<std::size_t> level_ns;
    std::size_t miss_ns = 0;
    std::size_t overhead_ns = 0;
};

struct RunResult
{
    std::size_t hits = 0;
    std::size_t misses = 0;
    std::vector<std::size_t> hits_by_level;
    std::size_t sim_ns = 0;
    double sim_ns_per_op = 0.0;
};

bool run(const std::vector<int>& sequence, const std::vector<LevelSpec>& levels, const AccessCosts& costs, RunResult& out);

#endif

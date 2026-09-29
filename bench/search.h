#ifndef SEARCH_H
#define SEARCH_H

#include "runner.h"
#include "workloads.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <utility>

struct Config
{
    std::vector<std::string> algorithms;
    std::size_t num_levels() const { return algorithms.size(); }
};

std::string config_to_string(const Config& c);

struct EvalResult
{
    double objective = 0.0;
    double mean_hit_rate = 0.0;
};

struct SearchOptions
{
    std::size_t ops_per_eval = 1000000;
    std::uint32_t seed = 42;
    std::vector<std::size_t> capacities;
    WorkloadType workload = WorkloadType::HotCold;
    std::string workload_label;
    std::vector<std::string> algorithm_pool;
    AccessCosts costs;
    int max_iterations = 30;
    int no_improvement_stop = 1;
    std::string log_csv;
};

struct SearchResult
{
    Config best;
    EvalResult result;
    int iterations = 0;
};

using ObjectiveFn = std::function<double(const Config&, EvalResult&)>;

SearchResult hill_climb(const Config& start, const ObjectiveFn& obj, const SearchOptions& opts);

#endif

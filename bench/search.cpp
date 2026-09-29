#include "search.h"

#include <fstream>
#include <set>
#include <sstream>

void add_algo_mutations(const Config& c, const std::vector<std::string>& pool, std::vector<Config>& out)
{
    for (std::size_t i = 0; i < c.algorithms.size(); ++i)
    {
        for (const auto& a : pool)
        {
            if (a == c.algorithms[i]) continue;
            Config n = c;
            n.algorithms[i] = a;
            out.push_back(std::move(n));
        }
    }
}

void add_level_grow(const Config& c, const std::vector<std::string>& pool, int max_levels, std::vector<Config>& out)
{
    if (static_cast<int>(c.algorithms.size()) >= max_levels) return;
    for (const auto& a : pool)
    {
        Config n = c;
        n.algorithms.push_back(a);
        out.push_back(std::move(n));
    }
}

void add_level_shrink(const Config& c, std::vector<Config>& out)
{
    if (c.algorithms.empty()) return;

    Config n = c;
    n.algorithms.pop_back();
    out.push_back(std::move(n));
}

std::vector<Config> generate_neighbors(const Config& c,
                                    const std::vector<std::string>& pool,
                                    int max_levels)
{
    std::vector<Config> out;
    out.reserve(32);
    add_algo_mutations(c, pool, out);
    add_level_grow(c, pool, max_levels, out);
    add_level_shrink(c, out);

    std::set<std::string> seen;
    std::vector<Config> unique;
    for (auto& n : out)
    {
        std::string key = config_to_string(n);
        if (seen.insert(key).second)
        {
            unique.push_back(std::move(n));
        }
    }
    return unique;
}

std::string config_to_string(const Config& c)
{
    std::ostringstream oss;
    for (std::size_t i = 0; i < c.algorithms.size(); ++i)
    {
        if (i) oss << '/';
        oss << c.algorithms[i];
    }
    return oss.str();
}

SearchResult hill_climb(const Config& start,
                        const ObjectiveFn& obj,
                        const SearchOptions& opts)
{
    std::ofstream log;
    if (!opts.log_csv.empty())
    {
        log.open(opts.log_csv, std::ios::app);
        if (log.tellp() == 0)
        {
            log << "workload,iteration,config,objective,hit_rate\n";
        }
    }

    const std::string wname = opts.workload_label.empty() ? workload_type_name(opts.workload) : opts.workload_label;

    SearchResult result;
    result.best = start;

    int no_improvement = 0;
    result.result.objective = obj(start, result.result);

    if (log.is_open())
    {
        log << wname << ',' << 0 << ','
            << '"' << config_to_string(start) << '"' << ','
            << result.result.objective << ','
            << result.result.mean_hit_rate << '\n';
    }

    const int max_levels = opts.capacities.size();

    for (int iter = 1; iter <= opts.max_iterations; ++iter)
    {
        auto neighbors = generate_neighbors(result.best,
                                            opts.algorithm_pool,
                                            max_levels);

        Config best_neighbor;
        EvalResult best_neighbor_result;
        double best_obj = result.result.objective;
        bool improved = false;

        for (const auto& cand : neighbors)
        {
            EvalResult r;
            double val = obj(cand, r);

            if (log.is_open())
            {
                log << wname << ',' << iter << ','
                    << '"' << config_to_string(cand) << '"' << ','
                    << val << ',' << r.mean_hit_rate << '\n';
            }

            if (val < best_obj)
            {
                best_obj = val;
                best_neighbor = cand;
                best_neighbor_result = r;
                improved = true;
            }
        }

        if (!improved)
        {
            no_improvement++;

            if (no_improvement >= opts.no_improvement_stop) break;
        }

        else
        {
            result.best = best_neighbor;
            result.result = best_neighbor_result;
            no_improvement = 0;
        }

        result.iterations = iter;
    }

    return result;
}

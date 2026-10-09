#include "workloads.h"
#include "runner.h"
#include "search.h"
#include "opt_cache.h"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>


struct Options
{
    std::size_t ops = 1000000;
    std::uint32_t seed = 1234;
    int max_iter = 20;
    int min_levels = 1;
    std::vector<std::size_t> capacities;
    std::vector<std::size_t> level_ns;
    std::size_t miss_ns = 0;
    std::size_t overhead_ns = 20;
    std::vector<std::string> algorithm_names;
    std::string csv_path;
    std::string workloads_path;
    std::vector<WorkloadSpec> workload_specs;
};


const char* known_algorithms[] = {"LRU", "LFU", "LIRS", "ARC", "2Q"};


bool is_valid_algorithm(const std::string& a)
{
    for (const char* x : known_algorithms)
    {
        if (a == x) return true;
    }
    return false;
}


template <typename T>
bool parse_num(const std::string& s, T& out)
{
    if (s.empty()) return false;

    const char* first = s.data();
    const char* last  = s.data() + s.size();

    auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc{} && ptr == last;
}


std::vector<std::string> default_names(const char** arr, std::size_t n)
{
    return std::vector<std::string>(arr, arr + n);
}


AccessCosts build_costs(const Options& opts, std::size_t levels)
{
    AccessCosts c;
    c.level_ns.resize(levels);

    for (std::size_t i = 0; i < levels; i++)
    {
        c.level_ns[i] = (i < opts.level_ns.size()) ? opts.level_ns[i] : opts.level_ns.back();
    }

    c.miss_ns = opts.miss_ns;
    c.overhead_ns = opts.overhead_ns;
    return c;
}


bool parse_indexed_flag(const std::string& arg,
                        const std::string& val,
                        std::map<int, std::size_t>& out,
                        std::string& err)
{
    long long v = 0;
    if (!parse_num(val, v) || v <= 0)
    {
        err = arg + " must be a positive integer, got '" + val + "'";
        return false;
    }

    int idx = 0;
    if (!parse_num(arg.substr(2), idx) || idx < 0 || idx > 63)
    {
        err = "bad index in " + arg;
        return false;
    }

    out[idx] = static_cast<std::size_t>(v);
    return true;
}


bool parse_args(int argc, char** argv, Options& o, std::string& err)
{
    std::map<int, std::size_t> caps_map;
    std::map<int, std::size_t> lat_map;

    for (int i = 1; i < argc; i++)
    {
        const std::string a = argv[i];
        std::string val;

        auto next = [&](std::string& out) -> bool
        {
            if (i + 1 >= argc)
            {
                err = "missing value for " + a;
                return false;
            }

            out = argv[++i];
            return true;
        };

        if (a.size() >= 3 && a[0] == '-' && a[1] == 'L' && std::isdigit(static_cast<unsigned char>(a[2])))
        {
            if (!next(val)) return false;
            if (!parse_indexed_flag(a, val, caps_map, err)) return false;
            continue;
        }

        if (a.size() >= 3 && a[0] == '-' && a[1] == 'T' && std::isdigit(static_cast<unsigned char>(a[2])))
        {
            if (!next(val)) return false;
            if (!parse_indexed_flag(a, val, lat_map, err)) return false;
            continue;
        }

        if (a == "--max-iter")
        {
            if (!next(val)) return false;
            long long v = 0;
            if (!parse_num(val, v) || v <= 0)
            {
                err = "--max-iter must be > 0, got '" + val + "'";
                return false;
            }
            o.max_iter = static_cast<int>(v);
        }

        else if (a == "--min-levels")
        {
            if (!next(val)) return false;
            long long v = 0;
            if (!parse_num(val, v) || v < 1)
            {
                err = "--min-levels must be >= 1, got '" + val + "'";
                return false;
            }
            o.min_levels = static_cast<int>(v);
        }

        else if (a == "--ops")
        {
            if (!next(val)) return false;
            long long v = 0;
            if (!parse_num(val, v))
            {
                err = "--ops must be a number, got '" + val + "'";
                return false;
            }
            o.ops = static_cast<std::size_t>(v);
        }

        else if (a == "--seed")
        {
            if (!next(val)) return false;
            unsigned long long v = 0;
            if (!parse_num(val, v))
            {
                err = "--seed must be a number, got '" + val + "'";
                return false;
            }
            o.seed = static_cast<std::uint32_t>(v);
        }

        else if (a == "--csv")
        {
            if (!next(val)) return false;
            if (val.empty())
            {
                err = "--csv must be non-empty";
                return false;
            }
            o.csv_path = val;
        }

        else if (a == "--miss-ns")
        {
            if (!next(val)) return false;
            long long v = 0;
            if (!parse_num(val, v) || v <= 0)
            {
                err = "--miss-ns must be > 0, got '" + val + "'";
                return false;
            }
            o.miss_ns = static_cast<std::size_t>(v);
        }

        else if (a == "--overhead-ns")
        {
            if (!next(val)) return false;
            long long v = 0;
            if (!parse_num(val, v) || v < 0)
            {
                err = "--overhead-ns must be >= 0, got '" + val + "'";
                return false;
            }
            o.overhead_ns = static_cast<std::size_t>(v);
        }

        else if (a == "--workloads")
        {
            if (!next(val)) return false;
            if (val.empty())
            {
                err = "--workloads must be non-empty";
                return false;
            }
            o.workloads_path = val;
        }

        else if (a == "--algorithm")
        {
            if (!next(val)) return false;
            if (!is_valid_algorithm(val))
            {
                err = "unknown algorithm: " + val;
                return false;
            }
            o.algorithm_names.push_back(val);
        }

        else
        {
            err = "unknown argument: " + a;
            return false;
        }
    }

    if (o.workloads_path.empty())
    {
        err = "--workloads PATH is required";
        return false;
    }

    if (caps_map.empty())
    {
        err = "at least -L0 must be given";
        return false;
    }

    if (lat_map.empty())
    {
        err = "at least -T0 must be given";
        return false;
    }
    if (o.miss_ns == 0)
    {
        err = "--miss-ns is required and must be > 0";
        return false;
    }

    const int n_caps = static_cast<int>(caps_map.size());
    for (int i = 0; i < n_caps; i++)
    {
        if (!caps_map.count(i))
        {
            err = "missing -L" + std::to_string(i);
            return false;
        }

        o.capacities.push_back(caps_map[i]);
    }

    const int n_lat = static_cast<int>(lat_map.size());
    for (int i = 0; i < n_lat; i++)
    {
        if (!lat_map.count(i))
        {
            err = "missing -T" + std::to_string(i);
            return false;
        }

        o.level_ns.push_back(lat_map[i]);
    }

    if (o.min_levels < 1 || o.min_levels > static_cast<int>(o.capacities.size()))
    {
        err = "--min-levels out of range: "
            + std::to_string(o.min_levels)
            + " (must be from 1 to" + std::to_string(o.capacities.size()) + ")";
        return false;
    }

    return true;
}


bool search_for_workload(const Options& opts,
                        const AccessCosts& costs,
                        const WorkloadSpec& spec,
                        SearchResult& result_out,
                        std::size_t& optimal_hits_out,
                        std::string& err)
{
    SearchOptions s;
    s.ops_per_eval = opts.ops;
    s.seed = opts.seed;
    s.max_iterations = opts.max_iter;
    s.no_improvement_stop = 1;
    s.log_csv = opts.csv_path;
    s.workload = spec.workload.type;
    s.workload_label = spec.name;
    s.capacities = opts.capacities;
    s.costs = costs;

    s.algorithm_pool = opts.algorithm_names.empty() ? default_names(known_algorithms, std::size(known_algorithms)) : opts.algorithm_names;

    Workload w = spec.workload;
    w.n = opts.ops;

    std::vector<int> seq;
    if (!generate(w, opts.seed, seq))
    {
        err = "failed to generate workload '" + spec.name + "'";
        return false;
    }

    std::size_t total_capacity = 0;
    for (auto c : opts.capacities) total_capacity += c;

    if (total_capacity == 0)
    {
        err = "total capacity is 0 for workload '" + spec.name + "'";
        return false;
    }

    OptimalCache<int, int> opt(total_capacity, seq);
    optimal_hits_out = static_cast<std::size_t>(opt.simulate());

    std::string run_err;

    ObjectiveFn obj = [&s, &seq, &run_err](const Config& c, EvalResult& out) -> double
    {
        if (c.algorithms.empty() || c.algorithms.size() > s.capacities.size())
        {
            return std::numeric_limits<double>::infinity();
        }

        std::vector<LevelSpec> levels;

        for (std::size_t i = 0; i < c.algorithms.size(); i++)
        {
            levels.push_back({c.algorithms[i], s.capacities[i]});
        }

        RunResult r;

        if (!run(seq, levels, s.costs, r))
        {
            run_err = "run() failed for config " + config_to_string(c);
            return std::numeric_limits<double>::infinity();
        }

        out.objective = r.sim_ns_per_op;
        out.mean_hit_rate = static_cast<double>(r.hits) / seq.size();


        return out.objective;
    };

    result_out.result.objective = std::numeric_limits<double>::infinity();

    for (const auto& start_algo : s.algorithm_pool)
    {
        Config start;
        start.algorithms.push_back(start_algo);


        SearchResult r = hill_climb(start, obj, s);

        if (!run_err.empty())
        {
            err = run_err;
            return false;
        }

        if (r.result.objective < result_out.result.objective)
        {
            result_out = std::move(r);
        }
    }

    return true;
}


bool run_search(const Options& opts, std::string& err)
{
    if (!opts.csv_path.empty())
    {
        std::ofstream trunc(opts.csv_path, std::ios::trunc);
        if (!trunc.is_open())
        {
            err = "cannot open csv for writing: " + opts.csv_path;
            return false;
        }

        trunc << "workload,iteration,config,objective,hit_rate\n";

        if (!trunc)
        {
            err = "write error on csv: " + opts.csv_path;
            return false;
        }
    }

    AccessCosts costs = build_costs(opts, opts.capacities.size());

    struct Outcome
    {
        std::string name;
        SearchResult result;
        std::size_t optimal_hits = 0;
        std::size_t total_ops = 0;
    };

    std::vector<Outcome> outcomes;

    for (const auto& spec : opts.workload_specs)
    {
        SearchResult r;
        std::size_t opt_hits = 0;

        if (!search_for_workload(opts, costs, spec, r, opt_hits, err))
        {
            return false;
        }

        outcomes.push_back({spec.name, std::move(r), opt_hits, opts.ops});
    }

    std::cout << std::left << std::setw(14) << "workload"
            << std::setw(24) << "algorithms"
            << std::right
            << std::setw(7) << "levels"
            << std::setw(11) << "hit_rate"
            << std::setw(11) << "opt_rate"
            << std::setw(12) << "sim_ns/op"
            << '\n';

    for (const auto& o : outcomes)
    {
        const double opt_rate = (o.total_ops == 0) ? 0.0 : static_cast<double>(o.optimal_hits) / o.total_ops;

        std::cout << std::left << std::setw(14) << o.name
                << std::setw(24) << config_to_string(o.result.best)
                << std::right
                << std::setw(7) << o.result.best.algorithms.size()
                << std::fixed << std::setprecision(4)
                << std::setw(11) << o.result.result.mean_hit_rate
                << std::setw(11) << opt_rate
                << std::setprecision(1)
                << std::setw(12) << o.result.result.objective
                << '\n';
    }

    return true;
}


int main(int argc, char** argv)
{
    Options opts;
    std::string err;

    if (!parse_args(argc, argv, opts, err))
    {
        std::cerr << "error: " << err << '\n';
        return 2;
    }

    if (!load_workload_specs(opts.workloads_path, opts.ops, opts.workload_specs, err))
    {
        std::cerr << "error: " << err << '\n';
        return 1;
    }

    if (!run_search(opts, err))
    {
        std::cerr << "error: " << err << '\n';
        return 1;
    }

    return 0;
}

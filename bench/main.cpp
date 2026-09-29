#include "workloads.h"
#include "runner.h"
#include "search.h"
#include "opt_cache.h"

#include <cctype>
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




const char* kAllAlgorithms[] = {"LRU", "LFU", "LIRS", "ARC", "2Q"};



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


void parse_indexed_flag(const std::string& arg, const std::string& val, std::map<int, std::size_t>& out)
{
    out[std::stoi(arg.substr(2))] = std::stoull(val);
}


void parse_args(int argc, char** argv, Options& o)
{
    std::map<int, std::size_t> caps_map;
    std::map<int, std::size_t> lat_map;

    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        auto next = [&](std::string& out) { out = argv[++i]; };
        std::string val;

        if (a.size() >= 3 && a[0] == '-' && a[1] == 'L' && std::isdigit(static_cast<unsigned char>(a[2])))
        {
            next(val);
            parse_indexed_flag(a, val, caps_map);
        }

        else if (a.size() >= 3 && a[0] == '-' && a[1] == 'T' && std::isdigit(static_cast<unsigned char>(a[2])))
        {
            next(val);
            parse_indexed_flag(a, val, lat_map);
        }

        else if (a == "--max-iter")    { next(val); o.max_iter = std::stoi(val); }
        else if (a == "--ops")         { next(val); o.ops = std::stoull(val); }
        else if (a == "--seed")        { next(val); o.seed = static_cast<std::uint32_t>(std::stoul(val)); }
        else if (a == "--csv")         { next(val); o.csv_path = val; }
        else if (a == "--miss-ns")     { next(val); o.miss_ns = std::stoull(val); }
        else if (a == "--overhead-ns") { next(val); o.overhead_ns = std::stoull(val); }
        else if (a == "--workloads")   { next(val); o.workloads_path = val; }
        else if (a == "--algorithm")   { next(val); o.algorithm_names.push_back(val); }
    }

    const int n_caps = static_cast<int>(caps_map.size());
    for (int i = 0; i < n_caps; i++) o.capacities.push_back(caps_map[i]);

    const int n_lat = static_cast<int>(lat_map.size());
    for (int i = 0; i < n_lat; i++) o.level_ns.push_back(lat_map[i]);
}


std::size_t search_for_workload(const Options& opts,
                                const AccessCosts& costs,
                                const WorkloadSpec& spec,
                                SearchResult& result_out)
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

    s.algorithm_pool = opts.algorithm_names.empty() ? default_names(kAllAlgorithms, std::size(kAllAlgorithms)) : opts.algorithm_names;

    Workload w = spec.workload;
    w.n = opts.ops;

    std::vector<int> seq;
    generate(w, opts.seed, seq);

    std::size_t total_capacity = 0;
    for (auto c : opts.capacities) total_capacity += c;

    OptimalCache<int, int> opt(total_capacity, seq);
    std::size_t optimal_hits = opt.simulate();

    ObjectiveFn obj = [&s, &seq](const Config& c, EvalResult& out) -> double
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

        run(seq, levels, s.costs, r);

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
        
        if (r.result.objective < result_out.result.objective)
        {
            result_out = std::move(r);
        }
    }


    return optimal_hits;
}

int run_search(const Options& opts)
{
    if (!opts.csv_path.empty())
    {
        std::ofstream trunc(opts.csv_path, std::ios::trunc);
        trunc << "workload,iteration,config,objective,hit_rate\n";
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
        std::size_t opt_hits = search_for_workload(opts, costs, spec, r);
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

    return 0;
}


int main(int argc, char** argv)
{
    Options opts;
    parse_args(argc, argv, opts);
    opts.workload_specs = load_workload_specs(opts.workloads_path, opts.ops);
    
    return run_search(opts);
}

#include "workloads.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <random>
#include <sstream>

int parse_int(const std::string& s)
{
    int v = 0;
    std::from_chars(s.data(), s.data() + s.size(), v);
    return v;
}

std::size_t parse_uint(const std::string& s)
{
    std::size_t v = 0;
    std::from_chars(s.data(), s.data() + s.size(), v);
    return v;
}

double parse_double(const std::string& s)
{
    double v = 0.0;
    std::from_chars(s.data(), s.data() + s.size(), v);
    return v;
}

std::vector<double> parse_alphas(const std::string& s)
{
    std::vector<double> out;
    std::istringstream iss(s);
    std::string token;

    while (std::getline(iss, token, ','))
    {
        if (token.empty()) continue;
        out.push_back(parse_double(token));
    }

    return out;
}









void gen_uniform(const Workload& w, std::uint32_t seed, std::vector<int>& out)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, w.key_max - 1);
    out.resize(w.n);
    for (auto& k : out) k = dist(rng);
}

void gen_zipf_single(const Workload& w, std::uint32_t seed, int key_max, double alpha, std::vector<int>& out)
{
    std::vector<double> cdf(key_max);
    double sum = 0.0;


    for (int i = 1; i <= key_max; i++)
    {
        sum += 1.0 / std::pow(static_cast<double>(i), alpha);
        cdf[i - 1] = sum;
    }


    for (auto& p : cdf) p /= sum;
    cdf.back() = 1.0;

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> u(0.0, 1.0);

    out.resize(w.n);

    for (auto& k : out)
    {
        double p = u(rng);
        auto it = std::lower_bound(cdf.begin(), cdf.end(), p);

        k = static_cast<int>(it - cdf.begin());
    }
}

void gen_zipf(const Workload& w, std::uint32_t seed, std::vector<int>& out)
{
    gen_zipf_single(w, seed, w.key_max, w.zipf_alpha, out);
}

void gen_zipf_multi(const Workload& w, std::uint32_t seed, std::vector<int>& out)
{
    out.clear();

    std::size_t phase = 0;

    while (out.size() < w.n)
    {
        Workload phase_w = w;
        phase_w.n = std::min(w.ops_per_phase, w.n - out.size());
        phase_w.zipf_alpha = w.zipf_alphas[phase % w.zipf_alphas.size()];

        std::vector<int> chunk;
        gen_zipf_single(phase_w,
                        seed + static_cast<std::uint32_t>(phase),
                        w.key_max,
                        phase_w.zipf_alpha,
                        chunk);

        out.insert(out.end(), chunk.begin(), chunk.end());
        phase++;
    }


    out.resize(w.n);
}

void gen_hotcold(const Workload& w, std::uint32_t seed, std::vector<int>& out)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    std::uniform_int_distribution<int> hot_pick(0, w.hot_keys - 1);
    std::uniform_int_distribution<int> cold_pick(0, w.cold_keys - 1);

    out.resize(w.n);
    for (auto& k : out)
    {
        if (coin(rng) < w.hot_ratio) k = hot_pick(rng);
        else k = w.hot_keys + cold_pick(rng);
    }
}

void gen_scan_stress(const Workload& w, std::uint32_t seed, std::vector<int>& out)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    std::uniform_int_distribution<int> hot_pick(0, w.hot_keys - 1);

    out.resize(w.n);
    int scan_pos = 0;
    for (auto& k : out)
    {
        if (coin(rng) < w.hot_ratio)
        {
            k = hot_pick(rng);
        }
        else
        {
            k = w.hot_keys + scan_pos;
            scan_pos = (scan_pos + 1) % w.scan_length;
        }
    }
}

void gen_phase_shift(const Workload& w, std::uint32_t seed, std::vector<int>& out)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> pick(0, w.keys_per_phase - 1);

    const std::size_t ops_per_phase = w.n / w.num_phases;
    out.resize(w.n);

    std::size_t idx = 0;
    for (int phase = 0; phase < w.num_phases && idx < w.n; ++phase)
    {
        const int base = phase * w.keys_per_phase;
        const std::size_t end = std::min(idx + ops_per_phase, w.n);
        for (; idx < end; idx++)
        {
            out[idx] = base + pick(rng);
        }
    }
}










bool workload_type_from_string(const std::string& s, WorkloadType& out)
{
    if (s == "uniform")     { out = WorkloadType::Uniform;    return true; }
    if (s == "zipf")        { out = WorkloadType::Zipf;       return true; }
    if (s == "zipfmulti")   { out = WorkloadType::ZipfMulti;  return true; }
    if (s == "hotcold")     { out = WorkloadType::HotCold;    return true; }
    if (s == "scan")        { out = WorkloadType::ScanStress; return true; }
    if (s == "phase")       { out = WorkloadType::PhaseShift; return true; }
    return false;
}

std::string workload_type_name(WorkloadType type)
{
    switch (type)
    {
        case WorkloadType::Uniform:    return "uniform";
        case WorkloadType::Zipf:       return "zipf";
        case WorkloadType::ZipfMulti:  return "zipf_multi";
        case WorkloadType::HotCold:    return "hotcold";
        case WorkloadType::ScanStress: return "scan_stress";
        case WorkloadType::PhaseShift: return "phase_shift";
    }
    return "?";
}

std::vector<WorkloadSpec> load_workload_specs(const std::string& path, std::size_t n)
{
    std::vector<WorkloadSpec> out;

    std::ifstream in(path);
    std::string line;

    while (std::getline(in, line))
    {
        if (auto p = line.find('#'); p != std::string::npos)
        {
            line.resize(p);
        }

        std::istringstream iss(line);
        std::string type;
        std::string name;
        if (!(iss >> type >> name)) continue;

        Workload w;
        w.n = n;
        workload_type_from_string(type, w.type);

        std::string kv;
        while (iss >> kv)
        {
            auto eq = kv.find('=');
            const std::string key = kv.substr(0, eq);
            const std::string val = kv.substr(eq + 1);

            if (key == "key_max")                           w.key_max = parse_int(val);
            else if (key == "alpha")                        w.zipf_alpha = parse_double(val);
            else if (key == "alphas")                       w.zipf_alphas = parse_alphas(val);
            else if (key == "ops_per_phase")                w.ops_per_phase = parse_uint(val);
            else if (key == "hot_ratio")                    w.hot_ratio = parse_double(val);
            else if (key == "hot_keys")                     w.hot_keys = parse_int(val);
            else if (key == "cold_keys")                    w.cold_keys = parse_int(val);
            else if (key == "scan_length")                  w.scan_length = parse_int(val);
            else if (key == "num_phases")                   w.num_phases = parse_int(val);
            else if (key == "keys_per_phase")               w.keys_per_phase = parse_int(val);
        }

        out.push_back({name, w});
    }

    return out;
}

void generate(const Workload& w, std::uint32_t seed, std::vector<int>& out)
{
    switch (w.type)
    {
        case WorkloadType::Uniform:    gen_uniform(w, seed, out);     break;
        case WorkloadType::Zipf:       gen_zipf(w, seed, out);        break;
        case WorkloadType::ZipfMulti:  gen_zipf_multi(w, seed, out);  break;
        case WorkloadType::HotCold:    gen_hotcold(w, seed, out);     break;
        case WorkloadType::ScanStress: gen_scan_stress(w, seed, out); break;
        case WorkloadType::PhaseShift: gen_phase_shift(w, seed, out); break;
    }
}

#include "workloads.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <random>
#include <sstream>
#include <string>


template <typename T>
bool parse_num(const std::string& s, T& out)
{
    if (s.empty()) return false;

    const char* first = s.data();
    const char* last  = s.data() + s.size();

    auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc{} && ptr == last;
}

bool parse_alphas(const std::string& s, std::vector<double>& out)
{
    out.clear();

    std::istringstream iss(s);
    std::string token;

    while (std::getline(iss, token, ','))
    {
        if (token.empty()) continue;

        double v{};
        if (!parse_num(token, v)) return false;
        out.push_back(v);
    }

    return !out.empty();
}

void gen_uniform(const Workload& w, std::uint32_t seed, std::vector<int>& out)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, w.key_max - 1);

    out.resize(w.n);
    for (auto& k : out) k = dist(rng);
}

void gen_zipf_single(const Workload& w,
                     std::uint32_t seed,
                     int key_max,
                     double alpha,
                     std::vector<int>& out)
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
        const double p = u(rng);
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
        else                         k = w.hot_keys + cold_pick(rng);
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

bool validate_workload(const Workload& w, std::string& err)
{
    if (w.n == 0)
    {
        err = "n must be > 0";
        return false;
    }

    switch (w.type)
    {
        case WorkloadType::Uniform:
            if (w.key_max <= 0)                     { err = "key_max must be > 0"; return false; }

            break;

        case WorkloadType::Zipf:
            if (w.key_max <= 0)                     { err = "key_max must be > 0"; return false; }
            if (w.zipf_alpha < 0.0)                 { err = "alpha must be >= 0"; return false; }

            break;

        case WorkloadType::ZipfMulti:
            if (w.key_max <= 0)                     { err = "key_max must be > 0"; return false; }
            if (w.zipf_alphas.empty())              { err = "alphas must be non-empty"; return false; }
            if (w.ops_per_phase == 0)               { err = "ops_per_phase must be > 0"; return false; }

            for (double a : w.zipf_alphas)
            {
                if (a < 0.0) { err = "each alpha must be >= 0"; return false; }
            }

            break;

        case WorkloadType::HotCold:
            if (w.hot_keys <= 0)                        { err = "hot_keys must be > 0"; return false; }
            if (w.cold_keys <= 0)                       { err = "cold_keys must be > 0"; return false; }
            if (w.hot_ratio < 0.0 || w.hot_ratio > 1.0) { err = "hot_ratio must be in [0,1]"; return false; }

            break;

        case WorkloadType::ScanStress:
            if (w.hot_keys <= 0)                        { err = "hot_keys must be > 0"; return false; }
            if (w.scan_length <= 0)                     { err = "scan_length must be > 0"; return false; }
            if (w.hot_ratio < 0.0 || w.hot_ratio > 1.0) { err = "hot_ratio must be in [0,1]"; return false; }

            break;

        case WorkloadType::PhaseShift:
            if (w.num_phases <= 0)                  { err = "num_phases must be > 0"; return false; }
            if (w.keys_per_phase <= 0)              { err = "keys_per_phase must be > 0"; return false; }

            break;
    }

    return true;
}

bool load_workload_specs(const std::string& path,
                         std::size_t n,
                         std::vector<WorkloadSpec>& out,
                         std::string& err)
{
    out.clear();

    std::ifstream in(path);
    if (!in.is_open())
    {
        err = "cannot open workloads file: " + path;
        return false;
    }

    std::string line;
    std::size_t line_no = 0;

    while (std::getline(in, line))
    {
        line_no++;

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

        if (!workload_type_from_string(type, w.type))
        {
            err = "line " + std::to_string(line_no) + ": unknown workload type '" + type + "'";
            return false;
        }

        std::string kv;
        while (iss >> kv)
        {
            const auto eq = kv.find('=');
            if (eq == std::string::npos)
            {
                err = "line " + std::to_string(line_no) + ": expected key=value, got '" + kv + "'";
                return false;
            }

            const std::string key = kv.substr(0, eq);
            const std::string val = kv.substr(eq + 1);

            bool ok = true;

            if      (key == "key_max")        ok = parse_num(val, w.key_max);
            else if (key == "alpha")          ok = parse_num(val, w.zipf_alpha);
            else if (key == "alphas")         ok = parse_alphas(val, w.zipf_alphas);
            else if (key == "ops_per_phase")  ok = parse_num(val, w.ops_per_phase);
            else if (key == "hot_ratio")      ok = parse_num(val, w.hot_ratio);
            else if (key == "hot_keys")       ok = parse_num(val, w.hot_keys);
            else if (key == "cold_keys")      ok = parse_num(val, w.cold_keys);
            else if (key == "scan_length")    ok = parse_num(val, w.scan_length);
            else if (key == "phases" || key == "num_phases")
                                              ok = parse_num(val, w.num_phases);
            else if (key == "keys_per_phase") ok = parse_num(val, w.keys_per_phase);
            else
            {
                err = "line " + std::to_string(line_no) + ": unknown key '" + key + "'";
                return false;
            }

            if (!ok)
            {
                err = "line " + std::to_string(line_no) + ": bad value for key '" + key + "': '" + val + "'";
                return false;
            }
        }

        std::string w_err;
        if (!validate_workload(w, w_err))
        {
            err = "line " + std::to_string(line_no) + ": workload '" + name + "' invalid: " + w_err;
            return false;
        }

        out.push_back({name, w});
    }

    if (out.empty())
    {
        err = "no workloads found in " + path;
        return false;
    }

    return true;
}

bool generate(const Workload& w, std::uint32_t seed, std::vector<int>& out)
{
    std::string err;
    if (!validate_workload(w, err)) return false;

    switch (w.type)
    {
        case WorkloadType::Uniform:    gen_uniform(w, seed, out);     break;
        case WorkloadType::Zipf:       gen_zipf(w, seed, out);        break;
        case WorkloadType::ZipfMulti:  gen_zipf_multi(w, seed, out);  break;
        case WorkloadType::HotCold:    gen_hotcold(w, seed, out);     break;
        case WorkloadType::ScanStress: gen_scan_stress(w, seed, out); break;
        case WorkloadType::PhaseShift: gen_phase_shift(w, seed, out); break;
    }

    return true;
}

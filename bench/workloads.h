#ifndef WORKLOADS_H
#define WORKLOADS_H

#include <cstdint>
#include <string>
#include <vector>

enum class WorkloadType
{
    Uniform,
    Zipf,
    ZipfMulti,
    HotCold,
    ScanStress,
    PhaseShift,
};

struct Workload
{
    WorkloadType type = WorkloadType::Uniform;
    std::size_t n = 0;
    int key_max = 0;
    double zipf_alpha = 0.0;
    std::vector<double> zipf_alphas;
    std::size_t ops_per_phase = 0;
    double hot_ratio = 0.0;
    int hot_keys = 0;
    int cold_keys = 0;
    int scan_length = 0;
    int num_phases = 0;
    int keys_per_phase = 0;
};

struct WorkloadSpec
{
    std::string name;
    Workload workload;
};

std::string workload_type_name(WorkloadType type);
bool workload_type_from_string(const std::string& s, WorkloadType& out);
bool load_workload_specs(const std::string& path, std::size_t n, std::vector<WorkloadSpec>& out, std::string& err);


// key_max > 0, alpha >= 0, alphas non-empty and > 0
// ops_per_phase > 0 for ZipfMulti
// hot_ratio in [0,1], hot_keys > 0, cold_keys > 0 for HotCold
// hot_ratio in [0,1], hot_keys > 0, scan_length > 0 for ScanStress
// num_phases > 0, keys_per_phase > 0 for PhaseShift
bool validate_workload(const Workload& w, std::string& err);

// false if the workload is not validated
bool generate(const Workload& w, std::uint32_t seed, std::vector<int>& out);

#endif

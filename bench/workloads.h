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
std::vector<WorkloadSpec> load_workload_specs(const std::string& path, std::size_t n);
void generate(const Workload& w, std::uint32_t seed, std::vector<int>& out);

#endif

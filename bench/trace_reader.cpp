#include "trace_reader.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace cache_bench
{

constexpr std::size_t record_size = 24;

// 24 bytes
//   
//  
//  uint32_t timestamp
//  uint64_t obj_id
//  uint32_t obj_size
//  int64_t  next_access_vtime

bool load_oracle_general(const std::string& path,
                        std::size_t max_records,
                        std::vector<int>& sequence,
                        TraceInfo& info,
                        std::string& err)
{
    sequence.clear();
    info = {};

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        err = "cannot open trace: " + path;
        return false;
    }

    std::unordered_map<std::uint64_t, int> id_to_int;

    int next_id = 0;
    char buf[record_size];

    while (in.read(buf, record_size))
    {
        std::uint64_t obj_id;
        std::memcpy(&obj_id, buf + 4, 8);

        auto [it, inserted] = id_to_int.emplace(obj_id, next_id);
        if (inserted) ++next_id;

        sequence.push_back(it->second);

        if (max_records != 0 && sequence.size() >= max_records) break;
    }

    const bool stopped_early = (max_records != 0 && sequence.size() >= max_records);

    if (!stopped_early && in.gcount() != 0)
    {
        err = "truncated record in trace: " + path;
        return false;
    }

    if (sequence.empty())
    {
        err = "trace has no records: " + path;
        return false;
    }

    info.records = sequence.size();
    info.unique_keys = id_to_int.size();
    return true;
}

} // namespace cache_bench

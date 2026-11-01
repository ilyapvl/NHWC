#ifndef TRACE_READER_H
#define TRACE_READER_H

#include <cstddef>
#include <string>
#include <vector>

namespace cache_bench
{

struct TraceInfo
{
    std::size_t records     = 0;
    std::size_t unique_keys = 0;
};

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
                        std::string& err);

} // namespace cache_bench

#endif // TRACE_READER_H

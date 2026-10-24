#ifndef CACHE_SYSTEM_H
#define CACHE_SYSTEM_H

#include "cache.h"
#include "lru_cache.h"
#include "lfu_cache.h"
#include "lirs_cache.h"
#include "arc_cache.h"
#include "twoq_cache.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <functional>
#include <utility>

namespace cache
{

enum cache_system_err
{
    INVALID_ALGORITHM = 1001,
};

template<typename K, typename V>
class CacheSystem
{
private:
    std::vector<std::unique_ptr<Cache<K, V>>> m_levels;
    
    std::function<V(const K&)> m_slow_get_page;
    std::vector<int> m_hits;
    int m_last_hit_level = 0;

    

public:
    explicit CacheSystem(std::function<V(const K&)> slow_get_page);

    std::vector<CacheAlgorithm> m_algorithms;
    int add_cache(CacheAlgorithm algorithm, std::size_t capacity);

    V access(const K& key);
    int get_hits(int level) const { return m_hits.at(level); }
    int get_last_hit_level() const { return m_last_hit_level; }
    const std::vector<std::unique_ptr<Cache<K, V>>>& levels() const
    {
        return m_levels;
    }

};

}

#include "cache_system.tpp"

#endif // CACHE_SYSTEM_H

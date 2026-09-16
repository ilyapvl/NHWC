#ifndef CACHE_SYSTEM_H
#define CACHE_SYSTEM_H

#include "lru_cache.h"
#include "lfu_cache.h"
#include "lirs_cache.h"
#include "arc_cache.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <functional>
#include <utility>

template<typename K, typename V>
std::unique_ptr<Cache<K, V>> make_cache(const std::string& algorithm, std::size_t capacity);

template<typename K, typename V>
class CacheSystem
{
private:
    std::vector<std::unique_ptr<Cache<K, V>>> m_levels;
    std::function<V(const K&)> m_slow_get_page;
    std::vector<int> m_hits;
    int m_last_hit_level = 0;

public:
    CacheSystem(std::vector<std::unique_ptr<Cache<K, V>>> levels, std::function<V(const K&)> slow_get_page);
    V access(const K key);
    int get_hits(int level) const { return m_hits.at(level); }
    int get_last_hit_level() const { return m_last_hit_level; }

};

#include "cache_system.tpp"

#endif // CACHE_SYSTEM_H

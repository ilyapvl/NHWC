#ifndef CACHE_SYSTEM_H
#define CACHE_SYSTEM_H

#include "lru_cache.h"
#include "lfu_cache.h"
#include "lirs_cache.h"
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
    std::vector<std::size_t> m_hits;

public:
    CacheSystem(std::vector<std::unique_ptr<Cache<K, V>>> levels, std::function<V(const K&)> slow_get_page);
    V access(const K key);
    std::size_t get_hits(std::size_t level) const { return m_hits.at(level); }

};

#include "cache_system.tpp"

#endif // CACHE_SYSTEM_H

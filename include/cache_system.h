#ifndef CACHE_SYSTEM_H
#define CACHE_SYSTEM_H

#include "lru_cache.h"
#include "lfu_cache.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <cstddef>

template<typename K, typename V>
std::unique_ptr<Cache<K, V>> make_cache(const std::string& algorithm, std::size_t capacity);

template<typename K, typename V>
class CacheSystem
{
private:
    std::vector<std::unique_ptr<Cache<K, V>>> m_levels;

public:
    CacheSystem(std::vector<std::unique_ptr<Cache<K, V>>> levels);
    std::optional<std::size_t> access(const K key);
};

#include "cache_system.tpp"

#endif // CACHE_SYSTEM_H

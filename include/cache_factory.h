#ifndef CACHE_FACTORY_H
#define CACHE_FACTORY_H

#include "cache.h"
#include "lru_cache.h"
#include "lfu_cache.h"
#include "lirs_cache.h"
#include "arc_cache.h"
#include "twoq_cache.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace cache
{

template <typename K, typename V>
std::unique_ptr<Cache<K, V>> make_cache(CacheAlgorithm algorithm, std::int64_t capacity)
{
    int err = validate_capacity(algorithm, capacity);
    if (err)
    {
        std::cerr << "Invalid capacity for cache " << algorithm_from_enum(algorithm) << ": " << capacity << std::endl;
        return nullptr;
    }
    
    const std::size_t cap = static_cast<std::size_t>(capacity);

    if (algorithm == CacheAlgorithm::LRU)
        return std::make_unique<LRUCache<K,V>>(cap);
    if (algorithm == CacheAlgorithm::LFU)
        return std::make_unique<LFUCache<K,V>>(cap);
    if (algorithm == CacheAlgorithm::LIRS)
        return std::make_unique<LIRSCache<K,V>>(cap);
    if (algorithm == CacheAlgorithm::ARC)
        return std::make_unique<ARCCache<K,V>>(cap);
    if (algorithm == CacheAlgorithm::TwoQ)
        return std::make_unique<TwoQCache<K,V>>(cap);

    return nullptr;
}


}



#endif

#ifndef LRU_CACHE_H
#define LRU_CACHE_H


#include <list>
#include <unordered_map>
#include <optional>
#include <cassert>

#include "cache.h"


template<typename K, typename V>
class LRUCache : public Cache<K, V>
{
private:
    std::list<K> m_order;
    std::unordered_map<K, typename std::list<K>::iterator> m_positions;

public:
    LRUCache(std::size_t capacity);
    using Cache<K, V>::m_capacity;

    bool contains(const K key) const override;
    void extract(const K key) override;
    
    std::optional<K> insert(const K key) override;
};

#include "lru_cache.tpp"


#endif // LRU_CACHE_H

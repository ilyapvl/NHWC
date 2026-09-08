#ifndef LFU_CACHE_H
#define LFU_CACHE_H


#include <map>
#include <list>
#include <unordered_map>
#include <utility>
#include <optional>

#include "cache.h"


template<typename K, typename V>
class LFUCache : public Cache<K, V>
{
private:
    std::map<int, std::list<K>> m_freq_to_keys;
    std::unordered_map<K, std::pair<int, typename std::list<K>::iterator>> m_key_to_pair;

    void increment_frequency(const K key);

public:
    LFUCache(std::size_t capacity);
    using Cache<K, V>::m_capacity;

    bool contains(const K key) const override;
    void extract(const K key) override;
    std::optional<K> insert(const K key) override;
};

#include "lfu_cache.tpp"

#endif // LFU_CACHE_H

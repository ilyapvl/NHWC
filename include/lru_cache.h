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
    using Cache<K, V>::m_values;

    bool contains(const K& key) const override;
    std::unique_ptr<const V> extract_ptr(const K& key) override;
    std::optional<V> get(const K& key) override;
    std::optional<std::pair<K, std::unique_ptr<const V>>> insert_ptr(const K& key, std::unique_ptr<const V> vptr) override;

    void dump(std::ostream& out) const override;


    const std::list<K>& debug_order() const { return m_order; }
    const std::unordered_map<K, typename std::list<K>::iterator>& debug_positions() const { return m_positions; }
};

#include "lru_cache.tpp"


#endif // LRU_CACHE_H

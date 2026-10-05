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

    struct State
    {
        int freq;
    };
    
    GhostInfo<K, State> m_ghost_info { m_capacity };

    void increment_frequency(const K& key);

public:
    LFUCache(std::size_t capacity);
    using Cache<K, V>::m_capacity;
    using Cache<K, V>::m_values;

    bool contains(const K& key) const override;
    std::unique_ptr<const V> extract_ptr(const K& key) override;
    std::optional<V> get(const K& key) override;
    std::optional<std::pair<K, std::unique_ptr<const V>>> insert_ptr(const K& key, std::unique_ptr<const V> vptr) override;

    void dump(std::ostream& out) const override;


    

    const std::map<int, std::list<K>>& debug_freq_to_keys() const
    {
        return m_freq_to_keys;
    }

    const std::unordered_map<K, std::pair<int, typename std::list<K>::iterator>>& debug_key_to_pair() const
    {
        return m_key_to_pair;
    }

    bool debug_in_ghost(const K& key) const
    {
        return m_ghost_info.debug_contains(key);
    }

    int debug_ghost_freq(const K& key) const
    {
        auto s = m_ghost_info.debug_get(key);
        return s ? s->freq : 0;
    }

    std::size_t debug_ghost_size() const { return m_ghost_info.debug_size(); }
    std::size_t debug_ghost_capacity() const { return m_ghost_info.debug_capacity(); }
};

#include "lfu_cache.tpp"

#endif // LFU_CACHE_H

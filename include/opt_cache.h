#ifndef OPTIMAL_CACHE_H
#define OPTIMAL_CACHE_H

#include "cache.h"
#include <unordered_map>
#include <list>
#include <vector>
#include <optional>
#include <cassert>
#include <cstddef>
#include <map> 

template<typename K, typename V>
class OptimalCache : public Cache<K, V>
{
private:
    std::vector<K> m_sequence;
    int m_current_index = 0;

    std::unordered_map<K, std::list<int>> m_future_positions;

    std::unordered_map<K, bool> m_is_resident;

    void build_future_positions();

    K select_element_to_remove();

    std::map<int, K> m_resident_by_next_use;
    std::unordered_map<K, int> m_key_to_next_use; 

public:
    OptimalCache(std::size_t capacity, const std::vector<K>& sequence);
    using Cache<K, V>::m_capacity;
    using Cache<K, V>::m_values;

    int simulate();

    bool contains(const K& key) const override;
    std::optional<V> get(const K& key) const override;
    std::optional<std::pair<K, V>> insert(const K& key, const V& value) override;
    void extract(const K& key) override;
    void touch(const K& key) override;

    void dump(std::ostream& out) const override;
};

#include "opt_cache.tpp"

#endif // OPTIMAL_CACHE_H

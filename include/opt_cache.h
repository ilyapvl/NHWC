#ifndef OPTIMAL_CACHE_H
#define OPTIMAL_CACHE_H

#include "cache.h"
#include <unordered_map>
#include <list>
#include <vector>
#include <cstddef>
#include <set>
#include <ostream>
#include <limits>
#include <iterator>

template<typename K, typename V>
class OptimalCache
{
private:
    std::size_t m_capacity;
    std::vector<K> m_sequence;
    int m_current_index = 0;

    std::unordered_map<K, std::list<int>> m_future_positions;

    std::unordered_map<K, bool> m_is_resident;

    void build_future_positions();

    K select_element_to_remove();

    std::set<std::pair<int, K>> m_resident_by_next_use;
    std::unordered_map<K, int> m_key_to_next_use; 

public:
    OptimalCache(std::size_t capacity, const std::vector<K>& sequence);

    int simulate();


    void dump(std::ostream& out) const;
};

#include "opt_cache.tpp"

#endif // OPTIMAL_CACHE_H

#ifndef TWOQ_CACHE_H
#define TWOQ_CACHE_H




#include "cache.h"
#include <list>
#include <unordered_map>
#include <optional>
#include <utility>
#include <cstddef>
#include <ostream>



template<typename K, typename V>
class TwoQCache : public Cache<K, V>
{
private:
    enum struct List { Q1, Q2, GHOST, None};

    struct Element_info
    {
        List list = List::None;
        bool is_resident = false;
        typename std::list<K>::iterator it;
    };

    std::list<K> m_q1;
    std::list<K> m_q2;
    std::list<K> m_ghost;

    std::size_t m_target_q1_size;
    std::size_t m_target_ghost_size;

    std::unordered_map<K, Element_info> m_element_infos;

    void remove_from_list(Element_info& info);
    void trim_ghost();
    std::optional<std::pair<K, V>> replace();

public:
    TwoQCache(std::size_t capacity);
    using Cache<K, V>::m_capacity;
    using Cache<K, V>::m_values;

    bool contains(const K key) const override;
    std::optional<V> get(const K key) const override;
    std::optional<std::pair<K, V>> insert(const K key, const V value, bool is_user_request) override;
    void extract(const K key) override;
};




#include "twoq_cache.tpp"


#endif // TWOQ_CACHE_h

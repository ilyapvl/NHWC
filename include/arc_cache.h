#ifndef ARC_CACHE_H
#define ARC_CACHE_H

#include "cache.h"
#include <list>
#include <unordered_map>
#include <optional>
#include <utility>
#include <cstddef>
#include <ostream>

template<typename K, typename V>
class ARCCache : public Cache<K, V>
{
private:
    enum class List { None, T1, T2, B1, B2 };

    struct Element_info
    {
        bool resident = false;
        List list = List::None;
        typename std::list<K>::iterator it;
    };

    std::list<K> m_t1; // recent
    std::list<K> m_t2; // frequent
    std::list<K> m_b1; // ghost for T1
    std::list<K> m_b2; // ghost for T2

    std::size_t m_target_t1_size;

    std::unordered_map<K, Element_info> m_element_infos;

    void remove_from_list(Element_info& info);
    void push_to_front(std::list<K>& lst, const K& key, Element_info& info, List list_type);
    std::optional<std::pair<K, V>> replace(const bool hit_in_b2);

public:
    ARCCache(std::size_t capacity);
    using Cache<K, V>::m_capacity;
    using Cache<K, V>::m_values;

    bool contains(const K key) const override;
    std::optional<V> get(const K key) const override;
    std::optional<std::pair<K, V>> insert(const K key, const V value, bool is_user_request = true) override;
    void extract(const K key) override;
};

#include "arc_cache.tpp"

#endif // ARC_CACHE_H

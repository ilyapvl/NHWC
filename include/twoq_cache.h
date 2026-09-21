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

    struct State
    {
        List list = List::Q1;
    };
    
    GhostInfo<K, State> m_system_ghost { m_capacity };

    std::list<K> m_q1;
    std::list<K> m_q2;
    std::list<K> m_ghost;

    std::size_t m_target_q1_size;
    std::size_t m_target_ghost_size;

    std::unordered_map<K, Element_info> m_element_infos;

    void remove_from_list(Element_info& info);
    void trim_ghost();
    std::optional<std::pair<K, std::unique_ptr<const V>>> replace();

public:
    TwoQCache(std::size_t capacity);
    using Cache<K, V>::m_capacity;
    using Cache<K, V>::m_values;

    bool contains(const K& key) const override;
    std::unique_ptr<const V> extract_ptr(const K& key) override;
    std::optional<V> get(const K& key) override;
    std::optional<std::pair<K, std::unique_ptr<const V>>> insert_ptr(const K& key, std::unique_ptr<const V> vptr) override;

    void dump(std::ostream& out) const override;
};




#include "twoq_cache.tpp"


#endif // TWOQ_CACHE_h

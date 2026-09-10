#ifndef LIRS_CACHE_H
#define LIRS_CACHE_H


#include <list>
#include <unordered_map>
#include <optional>
#include <cassert>

#include "cache.h"

template<typename K, typename V>
class LIRSCache : public Cache<K, V>
{
private:
    std::list<K> m_stack;
    std::list<K> m_queue;
    

    struct Element_info
    {
        bool resident = false;
        bool lir = false;

        bool in_stack = false;
        bool in_queue = false;

        std::list<K>::iterator stack_it;
        std::list<K>::iterator queue_it;
    };

    std::unordered_map<K, Element_info> m_element_infos;

    std::size_t m_lir_capacity;
    std::size_t m_resident_count;
    std::size_t m_lir_count;

    std::size_t get_lir_capacity(std::size_t capacity);
    void move_to_stack_front(const K key, Element_info& element_info);
    void move_to_queue_front(const K key, Element_info& element_info);
    void remove_from_queue(Element_info& element_info);
    void remove_from_stack(Element_info& element_info);

    void remove_hir_from_stack_bottom();
    void exctract_hir();
    void bottom_lir_to_hir();
    void hir_to_lir(Element_info& info);
    void restore_lir_after_extraction();


public:
    LIRSCache(std::size_t capacity);
    using Cache<K, V>::m_capacity;
    using Cache<K, V>::m_values;

    bool contains(const K key) const override;
    std::optional<V> get(const K key) const override;
    std::optional<std::pair<K, V>> insert(const K key, const V value, const bool is_user_request) override;
    void extract(const K key) override;

};




#include "lirs_cache.tpp"


#endif

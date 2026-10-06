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

    struct State
    {
        bool is_lir = false;
    };

    GhostInfo<K, State> m_system_ghost { m_capacity };

    std::unordered_map<K, Element_info> m_element_infos;

    std::size_t m_lir_capacity;
    std::size_t m_resident_count;
    std::size_t m_lir_count;

    

    std::size_t get_lir_capacity(std::size_t capacity);
    void move_to_stack_front(const K& key, Element_info& element_info);
    void move_to_queue_front(const K& key, Element_info& element_info);
    void remove_from_queue(Element_info& element_info);
    void remove_from_stack(Element_info& element_info);

    void remove_hir_from_stack_bottom();
    void exctract_hir();
    void bottom_lir_to_hir();
    void hir_to_lir(Element_info& info);
    void cut_stack();

    std::optional<K> restore_lir_after_extraction();
    std::optional<K> m_last_promoted; //FIXME - temporal solution


public:
    LIRSCache(std::size_t capacity);
    using Cache<K, V>::m_capacity;
    using Cache<K, V>::m_values;

    static constexpr std::size_t LIR_HIR_RATIO = 100;
    static constexpr std::size_t MIN_HIR = 1;

    bool contains(const K& key) const override;
    std::unique_ptr<const V> extract_ptr(const K& key) override;
    std::optional<V> get(const K& key) override;
    std::optional<std::pair<K, std::unique_ptr<const V>>> insert_ptr(const K& key, std::unique_ptr<const V> vptr) override;

    void dump(std::ostream& out) const override;




    const std::list<K>& debug_stack() const { return m_stack; }
    const std::list<K>& debug_queue() const { return m_queue; }

    const std::unordered_map<K, Element_info>& debug_element_infos() const
    {
        return m_element_infos;
    }

    std::size_t debug_lir_capacity()   const { return m_lir_capacity; }
    std::size_t debug_lir_count()      const { return m_lir_count; }
    std::size_t debug_resident_count() const { return m_resident_count; }

    bool debug_in_ghost(const K& key) const
    {
        return m_system_ghost.debug_contains(key);
    }

    bool debug_ghost_is_lir(const K& key) const
    {
        auto s = m_system_ghost.debug_get(key);
        return s && s->is_lir;
    }

    std::size_t debug_ghost_size()     const { return m_system_ghost.debug_size(); }
    std::size_t debug_ghost_capacity() const { return m_system_ghost.debug_capacity(); }

};




#include "lirs_cache.tpp"


#endif

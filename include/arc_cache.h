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

    struct State
    {
        List list = List::T1;
    };

    GhostInfo<K, State> m_system_ghost { m_capacity };

    std::list<K> m_t1; // recent
    std::list<K> m_t2; // frequent
    std::list<K> m_b1; // ghost for T1
    std::list<K> m_b2; // ghost for T2

    std::size_t m_target_t1_size;

    std::unordered_map<K, Element_info> m_element_infos;

    void remove_from_list(Element_info& info);
    void push_to_front(std::list<K>& lst, const K& key, Element_info& info, List list_type);
    std::optional<std::pair<K, std::unique_ptr<const V>>> replace(const bool hit_in_b2);

public:
    ARCCache(std::size_t capacity);
    using Cache<K, V>::m_capacity;
    using Cache<K, V>::m_values;

    bool contains(const K& key) const override;
    std::unique_ptr<const V> extract_ptr(const K& key) override;
    std::optional<V> get(const K& key) override;
    std::optional<std::pair<K, std::unique_ptr<const V>>> insert_ptr(const K& key, std::unique_ptr<const V> vptr) override;




    void dump(std::ostream& out) const override;



    const std::list<K>& debug_t1() const { return m_t1; }
    const std::list<K>& debug_t2() const { return m_t2; }
    const std::list<K>& debug_b1() const { return m_b1; }
    const std::list<K>& debug_b2() const { return m_b2; }


    // -1   not in element_infos
    // 0    List::None
    // 1    List::T1
    // 2    List::T2
    // 3    List::B1
    // 4    List::B2
    int debug_list_tag(const K& key) const
    {
        auto it = m_element_infos.find(key);
        if (it == m_element_infos.end()) return -1;
        return static_cast<int>(it->second.list);
    }


    bool debug_iterator_valid(const K& key) const
    {
        auto it = m_element_infos.find(key);
        if (it == m_element_infos.end()) return false;

        const auto& info = it->second;
        switch (info.list)
        {
            case List::T1:
                return std::find(m_t1.begin(), m_t1.end(), key) != m_t1.end() && *info.it == key;

            case List::T2:
                return std::find(m_t2.begin(), m_t2.end(), key) != m_t2.end() && *info.it == key;

            case List::B1:
                return std::find(m_b1.begin(), m_b1.end(), key) != m_b1.end() && *info.it == key;

            case List::B2:
                return std::find(m_b2.begin(), m_b2.end(), key) != m_b2.end() && *info.it == key;

            case List::None:
                return false;
        }

        return false;
    }

    std::size_t debug_target_t1_size() const { return m_target_t1_size; }

    std::vector<K> debug_all_keys() const
    {
        std::vector<K> out;

        for (const auto& [k, _] : m_element_infos) out.push_back(k);

        return out;
    }

    bool debug_in_ghost(const K& key) const
    {
        return m_system_ghost.debug_contains(key);
    }

    bool debug_ghost_is_t2(const K& key) const
    {
        auto s = m_system_ghost.debug_get(key);
        return s && s->list == List::T2;
    }
};

#include "arc_cache.tpp"

#endif // ARC_CACHE_H

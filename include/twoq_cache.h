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






    const std::list<K>& debug_q1()    const { return m_q1; }
    const std::list<K>& debug_q2()    const { return m_q2; }
    const std::list<K>& debug_ghost() const { return m_ghost; }

    std::size_t debug_target_q1_size()    const { return m_target_q1_size; }
    std::size_t debug_target_ghost_size() const { return m_target_ghost_size; }
    std::size_t debug_infos_size()        const { return m_element_infos.size(); }

    
    // 0    not found
    // 1    Q1
    // 2    Q2
    // 3    GHOST
    int debug_list_tag(const K& key) const
    {
        auto it = m_element_infos.find(key);
        if (it == m_element_infos.end()) return -1;

        switch (it->second.list)
        {
            case List::Q1:    return 1;
            case List::Q2:    return 2;
            case List::GHOST: return 3;
            case List::None:  return 0;
        }
        return 0;
    }

    bool debug_in_ghost(const K& key) const
    {
        return m_system_ghost.debug_contains(key);
    }

    bool debug_ghost_is_q2(const K& key) const
    {
        auto s = m_system_ghost.debug_get(key);
        return s && s->list == List::Q2;
    }

    // every element_infohas a valld iterator pointing at its key
    bool debug_iterators() const
    {
        for (const auto& [k, info] : m_element_infos)
        {
            switch (info.list)
            {
                case List::Q1:
                    if (info.it == m_q1.end() || *info.it != k) return false;
                    break;

                case List::Q2:
                    if (info.it == m_q2.end() || *info.it != k) return false;
                    break;

                case List::GHOST:
                    if (info.it == m_ghost.end() || *info.it != k) return false;
                    break;

                case List::None:
                    return false;
            }
        }





        return true;
    }
};




#include "twoq_cache.tpp"


#endif // TWOQ_CACHE_h

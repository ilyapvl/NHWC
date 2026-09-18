#ifndef GHOST_INFO_HPP
#define GHOST_INFO_HPP

#include <list>
#include <unordered_map>
#include <optional>
#include <cstddef>

template<typename K, typename S>
class GhostInfo
{
    struct Entry
    {
        S state;
        typename std::list<K>::iterator it;
    };

    std::unordered_map<K, Entry> m_map;
    std::list<K> m_order;
    std::size_t m_size;

    void trim()
    {
        while (m_map.size() > m_size)
        {
            K victim = m_order.back();
            m_order.pop_back();
            m_map.erase(victim);
        }
    }

public:
    GhostInfo(std::size_t size) : m_size(size) {}

    void save(const K& key, S state)
    {
        auto it = m_map.find(key);
        
        if (it != m_map.end())
        {
            m_order.erase(it->second.it);
            m_map.erase(it);
        }

        m_order.push_front(key);
        m_map[key] = Entry{state, m_order.begin()};

        trim();
    }

    std::optional<S> pop(const K& key)
    {
        auto it = m_map.find(key);
        if (it == m_map.end()) return std::nullopt;

        S s = it->second.state;
        m_order.erase(it->second.it);
        m_map.erase(it);

        return s;
    }
};

#endif

#ifndef OPTIMAL_CACHE_TPP
#define OPTIMAL_CACHE_TPP


template <typename K, typename V>
OptimalCache<K, V>::OptimalCache(std::size_t capacity, const std::vector<K>& sequence)
    : Cache<K, V>(capacity), m_sequence(sequence)
{
    build_future_positions();
}

template <typename K, typename V>
void OptimalCache<K, V>::build_future_positions()
{
    for (int i = 0; i < m_sequence.size(); i++)
    {
        m_future_positions[m_sequence[i]].push_back(i);
    }
}


template <typename K, typename V>
int OptimalCache<K, V>::simulate()
{
    int hits = 0;

    for (std::size_t i = 0; i < m_sequence.size(); i++)
    {
        const K& key = m_sequence[i];
        m_current_index = i;



        auto it_future = m_future_positions.find(key);
        if (it_future != m_future_positions.end() && !it_future->second.empty())
        {
            it_future->second.pop_front();
        }

        if (m_key_to_next_use.find(key) != m_key_to_next_use.end())
        {
            hits++;

            int old_next = m_key_to_next_use[key];
            m_resident_by_next_use.erase(old_next);
            m_key_to_next_use.erase(key);

            int new_next = std::numeric_limits<int>::max();

            if (it_future != m_future_positions.end() && !it_future->second.empty())
            {
                new_next = it_future->second.front();
            }
            m_key_to_next_use[key] = new_next;
            m_resident_by_next_use[key] = key;
        }

        else
        {
            if (m_key_to_next_use.size() == m_capacity)
            {
                auto it_last_resident = m_resident_by_next_use.end();
                it_last_resident--;

                K element_to_erase = it_last_resident->second;
                m_key_to_next_use.erase(element_to_erase);
                m_resident_by_next_use.erase(element_to_erase);
                m_values.erase(element_to_erase);
            }
            


            int new_next_use = std::numeric_limits<int>::max();

            if (it_future != m_future_positions.end() && !it_future->second.empty())
            {
                new_next_use = it_future->second.front();
            }
            m_key_to_next_use[key] = new_next_use;
            m_resident_by_next_use[new_next_use] = key;
        }



        // dump(std::cout);
    }

    return hits;
}



template<typename K, typename V>
bool OptimalCache<K, V>::contains(const K& key) const
{
    return m_key_to_next_use.find(key) != m_key_to_next_use.end();
}

template<typename K, typename V>
std::optional<V> OptimalCache<K, V>::get(const K& key) const
{
    return std::nullopt;
}

template<typename K, typename V>
std::optional<std::pair<K, V>> OptimalCache<K, V>::insert(const K& key, const V& value)
{
    return std::nullopt;
}

template<typename K, typename V>
void OptimalCache<K, V>::extract(const K& key) {};


template<typename K, typename V>
void OptimalCache<K, V>::touch(const K& key) {};




template<typename K, typename V>
void OptimalCache<K, V>::dump(std::ostream& out) const
{
    out << "OptimalCache (capacity=" << m_capacity
        << ", resident=" << m_is_resident.size()
        << ", next_index=" << m_current_index
        << "/" << m_sequence.size() << ")\n";

    out << "    resident [key(next use position)]: ";
    bool first = true;
    for (const auto& [key, _] : m_is_resident) {
        if (!first) out << ", ";
        first = false;

        out << key;
        auto it = m_future_positions.find(key);
        if (it != m_future_positions.end() && !it->second.empty())
            out << "(next@" << it->second.front() << ")";
        else
            out << "(never)";
    }
    out << '\n';
}


#endif // OPTIMAL_CACHE_TPP

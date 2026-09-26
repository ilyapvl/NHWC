#ifndef OPTIMAL_CACHE_TPP
#define OPTIMAL_CACHE_TPP


template <typename K, typename V>
OptimalCache<K, V>::OptimalCache(std::size_t capacity, const std::vector<K>& sequence)
    : m_capacity(capacity), m_sequence(sequence)
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
    if (m_capacity == 0) return 0;
    
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

        int new_next_use = std::numeric_limits<int>::max();
        if (it_future != m_future_positions.end() && !it_future->second.empty())
        {
            new_next_use = it_future->second.front();
        }

        auto present_it = m_key_to_next_use.find(key);

        if (present_it != m_key_to_next_use.end())
        {
            hits++;

            int old_next = m_key_to_next_use[key];
            m_resident_by_next_use.erase({old_next, key});
            m_key_to_next_use.erase(present_it);

        }

        else if (m_key_to_next_use.size() == m_capacity && !m_resident_by_next_use.empty())
        {
            auto last = std::prev(m_resident_by_next_use.end());
            K victim = last->second;
            m_resident_by_next_use.erase(last);
            m_key_to_next_use.erase(victim);
        }
            
        m_key_to_next_use[key] = new_next_use;
        m_resident_by_next_use.insert({new_next_use, key});



        // dump(std::cout);
    }

    return hits;
}


template<typename K, typename V>
void OptimalCache<K, V>::dump(std::ostream& out) const
{
    out << "OptimalCache (capacity=" << m_capacity
        << ", resident=" << m_is_resident.size()
        << ", next_index=" << m_current_index
        << "/" << m_sequence.size() << ")\n";

    out << "    resident [key(next use position)]: ";
    bool first = true;
    for (const auto& [key, next] : m_key_to_next_use)
    {
        if (!first) out << ", ";
        first = false;

        if (next == std::numeric_limits<int>::max())
            out << key << "(never)";
        else
            out << key << "(next@" << next << ")";
    }
    out << '\n';
}


#endif // OPTIMAL_CACHE_TPP

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
K OptimalCache<K, V>::select_element_to_remove()
{
    K element_to_remove;
    int farthest = 0;
    bool found = false;


    for (const auto& [key, resident] : m_is_resident)
    {
        auto it = m_future_positions.find(key);

        if (it == m_future_positions.end() || it->second.empty())
        {
            return key;
        }


        int next_use = it->second.front();

        if (!found || next_use > farthest)
        {
            farthest = next_use;
            element_to_remove = key;

            found = true;
        }
    }

    assert(found);

    return element_to_remove;

}



template <typename K, typename V>
int OptimalCache<K, V>::simulate()
{
    int hits = 0;

    for (std::size_t i = 0; i < m_sequence.size(); i++)
    {
        const K& key = m_sequence[i];
        m_current_index = i;



        auto it = m_future_positions.find(key);
        if (it != m_future_positions.end() || it->second.empty())
        {
            it->second.pop_front();
        }

        if (m_is_resident.find(key) != m_is_resident.end())
        {
            hits++;
        }

        else
        {
            if (m_is_resident.size() == m_capacity)
            {
                K element_to_erase = select_element_to_remove();
                m_is_resident.erase(element_to_erase);
                m_values.erase(element_to_erase);
            }
            


            m_is_resident[key] = true;
        }
    }

    return hits;
}



template<typename K, typename V>
bool OptimalCache<K, V>::contains(const K key) const
{
    return m_is_resident.find(key) != m_is_resident.end();
}

template<typename K, typename V>
std::optional<V> OptimalCache<K, V>::get(const K key) const
{
    return std::nullopt;
}

template<typename K, typename V>
std::optional<std::pair<K, V>> OptimalCache<K, V>::insert(const K key, const V value, const bool is_user_request)
{
    return std::nullopt;
}

template<typename K, typename V>
void OptimalCache<K, V>::extract(const K key) {};

#endif // OPTIMAL_CACHE_TPP

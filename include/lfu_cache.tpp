#ifndef LFU_CACHE_TPP
#define LFU_CACHE_TPP

template<typename K, typename V>
LFUCache<K, V>::LFUCache(std::size_t capacity) : Cache<K, V>(capacity) {}

template<typename K, typename V>
void LFUCache<K, V>::increment_frequency(const K key)
{
    auto& [freq, it] = m_key_to_pair[key];

    auto& old_list = m_freq_to_keys[freq];
    old_list.erase(it);

    if (old_list.empty()) m_freq_to_keys.erase(freq);

    freq++;

    auto& new_list = m_freq_to_keys[freq];
    new_list.push_front(key);
    it = new_list.begin();
}

template<typename K, typename V>
bool LFUCache<K, V>::contains(const K key) const
{
    return m_key_to_pair.find(key) != m_key_to_pair.end();
}

template<typename K, typename V>
void LFUCache<K, V>::extract(const K key)
{
    auto it = m_key_to_pair.find(key);
    if (it == m_key_to_pair.end()) return;

    int freq = it->second.first;
    auto list_it = it->second.second;
    m_freq_to_keys[freq].erase(list_it);

    if (m_freq_to_keys[freq].empty())
    {
        m_freq_to_keys.erase(freq);
    }

    m_values.erase(key);
    m_key_to_pair.erase(it);
}

template<typename K, typename V>
std::optional<std::pair<K, V>> LFUCache<K, V>::insert(const K key, const V value)
{
    if (contains(key))
    {
        increment_frequency(key);
        m_values[key] = value;
        return std::nullopt;
    }

    std::optional<std::pair<K, V>> element_to_erase;

    if (m_key_to_pair.size() == m_capacity)
    {
        auto& min_freq_list = m_freq_to_keys.begin()->second;
        element_to_erase = std::make_pair(min_freq_list.back(), m_values[min_freq_list.back()]);
        min_freq_list.pop_back();

        if (min_freq_list.empty())
        {
            m_freq_to_keys.erase(m_freq_to_keys.begin());
        }

        
        m_key_to_pair.erase(element_to_erase->first);
        m_values.erase(element_to_erase->first);
        
    }

    m_freq_to_keys[1].push_front(key);
    m_key_to_pair.emplace(key, std::make_pair(1, m_freq_to_keys[1].begin()));
    m_values[key] = value;

    return element_to_erase;
}

template<typename K, typename V>
std::optional<V> LFUCache<K, V>::get(const K key) const
{
    auto it = m_values.find(key);
    if (it != m_values.end())
    {
        return it->second;
    }

    return std::nullopt;
}

#endif // LFU_CACHE_TPP

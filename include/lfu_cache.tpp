#ifndef LFU_CACHE_TPP
#define LFU_CACHE_TPP

template<typename K, typename V>
LFUCache<K, V>::LFUCache(std::size_t capacity) : Cache<K, V>(capacity) {}

template<typename K, typename V>
void LFUCache<K, V>::increment_frequency(const K& key)
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
bool LFUCache<K, V>::contains(const K& key) const
{
    return m_key_to_pair.find(key) != m_key_to_pair.end();
}

template<typename K, typename V>
std::unique_ptr<const V> LFUCache<K, V>::extract_ptr(const K& key)
{
    auto it = m_key_to_pair.find(key);
    if (it == m_key_to_pair.end()) return nullptr;

    int freq = it->second.first;
    m_ghost_info.save(key, State{freq});

    auto list_it = it->second.second;
    auto vit = m_values.find(key);
    auto vptr = std::move(vit->second);

    m_freq_to_keys[freq].erase(list_it);

    if (m_freq_to_keys[freq].empty())
    {
        m_freq_to_keys.erase(freq);
    }


    m_values.erase(vit);
    m_key_to_pair.erase(it);

    

    return vptr;
}

template<typename K, typename V>
std::optional<std::pair<K, std::unique_ptr<const V>>> LFUCache<K, V>::insert_ptr(const K& key, std::unique_ptr<const V> vptr)
{
    if (!vptr) return std::nullopt;
    if (contains(key))
    {
        increment_frequency(key);
        m_values[key] = std::move(vptr);
        return std::nullopt;
    }

    std::optional<std::pair<K, std::unique_ptr<const V>>> erased;

    if (m_key_to_pair.size() == m_capacity && !m_freq_to_keys.empty())
    {
        auto& min_freq_list = m_freq_to_keys.begin()->second;

        K victim_key = min_freq_list.back();

        auto vit = m_values.find(victim_key);
        auto victim_value = std::move(vit->second);

        min_freq_list.pop_back();

        if (min_freq_list.empty())
        {
            m_freq_to_keys.erase(m_freq_to_keys.begin());
        }

        m_key_to_pair.erase(victim_key);
        m_values.erase(vit);

        erased = std::make_pair(victim_key, std::move(victim_value));
    }

    int freq = 1;

    auto g = m_ghost_info.pop(key);


    if (g.has_value())
    {
        freq = g.value().freq;
    }

    m_freq_to_keys[freq].push_front(key);
    m_key_to_pair.emplace(key, std::make_pair(freq, m_freq_to_keys[freq].begin()));
    m_values[key] = std::move(vptr);

    return erased;
}

template<typename K, typename V>
std::optional<V> LFUCache<K, V>::get(const K& key)
{
    auto it = m_key_to_pair.find(key);
    if (it == m_key_to_pair.end()) return std::nullopt;

    increment_frequency(key);

    return *m_values.at(key);
}






template<typename K, typename V>
void LFUCache<K, V>::dump(std::ostream& out) const
{
    out << "LFUCache (size=" << m_key_to_pair.size()
        << "/" << m_capacity << ")\n";

    for (const auto& [freq, keys] : m_freq_to_keys) {
        out << "    freq " << freq << " [MRU -> LRU]: ";
        bool first = true;
        for (const auto& key : keys) {
            if (!first) out << ", ";
            first = false;
            out << key;

        }
        out << '\n';
    }
}





#endif // LFU_CACHE_TPP

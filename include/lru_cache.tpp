#ifndef LRU_CACHE_TPP
#define LRU_CACHE_TPP

template<typename K, typename V>
LRUCache<K, V>::LRUCache(std::size_t capacity) : Cache<K, V>(capacity) {}

template<typename K, typename V>
bool LRUCache<K, V>::contains(const K key) const
{
    return m_positions.find(key) != m_positions.end();
}

template<typename K, typename V>
void LRUCache<K, V>::extract(const K key)
{
    auto it = m_positions.find(key);

    assert(it != m_positions.end());

    m_order.erase(it->second);
    m_positions.erase(it);
    m_values.erase(key);
}

template<typename K, typename V>
std::optional<std::pair<K, V>> LRUCache<K, V>::insert(const K key, const V value, const bool is_user_request)
{
    auto it = m_positions.find(key);

    if (it != m_positions.end())
    {
        m_order.splice(m_order.begin(), m_order, it->second);
        m_values[key] = value;
        return std::nullopt;
    }

    std::optional<std::pair<K,V>> element_to_erase;

    if (m_order.size() == m_capacity)
    {
        element_to_erase = std::make_pair(m_order.back(), m_values[m_order.back()]);
        m_positions.erase(element_to_erase->first);
        m_values.erase(element_to_erase->first);
        m_order.pop_back();
    }

    m_order.push_front(key);
    m_positions.emplace(key, m_order.begin());
    m_values[key] = value;

    return element_to_erase;
}

template<typename K, typename V>
std::optional<V> LRUCache<K, V>::get(const K key) const
{
    auto it = m_values.find(key);
    if (it != m_values.end())
    {
        return it->second;
    }

    return std::nullopt;
}






template<typename K, typename V>
void LRUCache<K, V>::dump(std::ostream& out) const
{
    out << "LRUCache (size=" << m_order.size()
        << "/" << m_capacity << ")\n";

    out << "    order [MRU -> LRU]: ";
    bool first = true;
    for (const auto& key : m_order) {
        if (!first) out << ", ";
        first = false;

        auto it = m_values.find(key);
        if (it != m_values.end())
            out << key;
        else
            out << key << "=NOVALUE ";
    }
    out << '\n';
}

#endif // LRU_CACHE_TPP

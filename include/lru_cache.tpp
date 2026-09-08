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
}

template<typename K, typename V>
std::optional<K> LRUCache<K, V>::insert(const K key)
{
    if (m_capacity == 0) return key;

    auto it = m_positions.find(key);

    if (it != m_positions.end())
    {
        m_order.splice(m_order.begin(), m_order, it->second);
        return std::nullopt;
    }

    std::optional<K> element_to_erase;

    if (m_order.size() == m_capacity)
    {
        element_to_erase = m_order.back();
        m_positions.erase(*element_to_erase);
        m_order.pop_back();
    }

    m_order.push_front(key);
    m_positions.emplace(key, m_order.begin());

    return element_to_erase;
}

#endif // LRU_CACHE_TPP

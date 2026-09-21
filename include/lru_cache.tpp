#ifndef LRU_CACHE_TPP
#define LRU_CACHE_TPP

template<typename K, typename V>
LRUCache<K, V>::LRUCache(std::size_t capacity) : Cache<K, V>(capacity) {}

template<typename K, typename V>
bool LRUCache<K, V>::contains(const K& key) const
{
    return m_positions.find(key) != m_positions.end();
}

template<typename K, typename V>
std::unique_ptr<const V> LRUCache<K, V>::extract_ptr(const K& key)
{
    auto it = m_positions.find(key);

    assert(it != m_positions.end());

    auto vit = m_values.find(key);
    auto vptr = (vit != m_values.end()) ? std::move(vit->second) : nullptr;

    m_order.erase(it->second);
    m_positions.erase(it);
    m_values.erase(key);

    return vptr;
}

template<typename K, typename V>
std::optional<std::pair<K, std::unique_ptr<const V>>> LRUCache<K, V>::insert_ptr(const K& key, std::unique_ptr<const V> vptr)
{
    if (!vptr) return std::nullopt;
    if (m_capacity == 0) return std::make_pair(key, std::move(vptr));

    auto it = m_positions.find(key);

    if (it != m_positions.end())
    {
        m_order.splice(m_order.begin(), m_order, it->second);
        m_values[key] = std::move(vptr);
        return std::nullopt;
    }

    std::optional<std::pair<K, std::unique_ptr<const V>>> erased;

    if (m_order.size() == m_capacity)
    {
        K victim_key = m_order.back();

        auto vit = m_values.find(victim_key);
        auto vptr = std::move(vit->second);

        m_positions.erase(victim_key);
        m_values.erase(vit);
        m_order.pop_back();

        erased = std::make_pair(victim_key, std::move(vptr));
    }

    m_order.push_front(key);
    m_positions.emplace(key, m_order.begin());
    m_values[key] = std::move(vptr);

    return erased;
}

template<typename K, typename V>
std::optional<V> LRUCache<K, V>::get(const K& key)
{
    auto it = m_positions.find(key);
    if (it == m_positions.end())
    {
        return std::nullopt;
    }

    m_order.splice(m_order.begin(), m_order, it->second);

    auto vit = m_values.find(key);
    if (vit == m_values.end() || !vit->second) return std::nullopt;
    
    return *vit->second;
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

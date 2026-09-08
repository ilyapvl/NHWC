#ifndef CACHE_SYSTEM_TPP
#define CACHE_SYSTEM_TPP

template<typename K, typename V>
std::unique_ptr<Cache<K, V>> make_cache(const std::string& algorithm, std::size_t capacity)
{
    if (algorithm == "LRU")
    {
        return std::make_unique<LRUCache<K, V>>(capacity);
    }
    else if (algorithm == "LFU")
    {
        return std::make_unique<LFUCache<K, V>>(capacity);
    }

    assert(false);
}

template<typename K, typename V>
CacheSystem<K, V>::CacheSystem(std::vector<std::unique_ptr<Cache<K, V>>> levels)
    : m_levels(std::move(levels))
{
}

template<typename K, typename V>
std::optional<std::size_t> CacheSystem<K, V>::access(const K key)
{
    std::optional<std::size_t> hit_level;

    for (std::size_t i = 0; i < m_levels.size(); i++)
    {
        if (m_levels[i]->contains(key))
        {
            hit_level = i;
            break;
        }
    }

    if (hit_level.has_value())
    {
        if (*hit_level == 0)
        {
            m_levels[*hit_level]->insert(key);
            return hit_level;
        }

        m_levels[*hit_level]->extract(key);
    }

    std::optional moving_key = key;

    for (std::size_t i = 0; i < m_levels.size() && moving_key.has_value(); i++)
    {
        moving_key = m_levels[i]->insert(*moving_key);
    }

    return hit_level;
}

#endif // CACHE_SYSTEM_TPP

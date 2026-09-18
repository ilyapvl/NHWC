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
    else if (algorithm == "LIRS")
    {
        return std::make_unique<LIRSCache<K, V>>(capacity);
    }
    else if (algorithm == "ARC")
    {
        return std::make_unique<ARCCache<K, V>>(capacity);
    }
    else if (algorithm == "2Q")
    {
        return std::make_unique<TwoQCache<K, V>>(capacity);
    }

    assert(false);
}

template<typename K, typename V>
CacheSystem<K, V>::CacheSystem(std::vector<std::unique_ptr<Cache<K, V>>> levels, std::function<V(const K&)> slow_get_page)
    : m_levels(std::move(levels)),
      m_slow_get_page(std::move(slow_get_page)),
      m_hits(m_levels.size(), 0)
{
}

template<typename K, typename V>
V CacheSystem<K, V>::access(const K& key)
{
    for (int i = 0; i < m_levels.size(); i++)
    {
        if (m_levels[i]->contains(key))
        {
            std::optional<V> maybe_value = m_levels[i]->get(key);
            if (!maybe_value.has_value()) continue;

            V value = maybe_value.value();


            if (i > 0)
            {
                m_levels[i]->touch(key); 
                m_levels[i]->extract(key);
            }


            std::optional<std::pair<K, V>> moving = std::make_pair(key, value);
            for (int j = 0; j < m_levels.size() && moving.has_value(); j++)
            {
                moving = m_levels[j]->insert(moving->first, moving->second);
            }

            m_last_hit_level = i;
            m_hits[i]++;
            return value;
        }


    }

    V value = m_slow_get_page(key);


    std::optional<std::pair<K, V>> moving = std::make_pair(key, value);
    for (std::size_t j = 0; j < m_levels.size() && moving.has_value(); j++)
    {
        moving = m_levels[j]->insert(moving->first, moving->second);
    }

    return value;
    
}

#endif // CACHE_SYSTEM_TPP

#ifndef CACHE_SYSTEM_TPP
#define CACHE_SYSTEM_TPP

template<typename K, typename V>
std::unique_ptr<Cache<K, V>> CacheSystem<K, V>::add_cache(const std::string& algorithm, std::size_t capacity)
{
    
    if (algorithm == "LRU")
    {
        m_levels.push_back(std::make_unique<LRUCache<K, V>>(capacity));
    }
    else if (algorithm == "LFU")
    {
        m_levels.push_back(std::make_unique<LFUCache<K, V>>(capacity));
    }
    else if (algorithm == "LIRS")
    {
        m_levels.push_back(std::make_unique<LIRSCache<K, V>>(capacity));
    }
    else if (algorithm == "ARC")
    {
        m_levels.push_back(std::make_unique<ARCCache<K, V>>(capacity));
    }
    else if (algorithm == "2Q")
    {
        m_levels.push_back(std::make_unique<TwoQCache<K, V>>(capacity));
    }

    return nullptr;
}

template<typename K, typename V>
CacheSystem<K, V>::CacheSystem(std::size_t size, std::function<V(const K&)> slow_get_page)
    : m_slow_get_page(std::move(slow_get_page)),
      m_hits(size, 0)
{
}

template<typename K, typename V>
V CacheSystem<K, V>::access(const K& key)
{
    for (int i = 0; i < m_levels.size(); i++)
    {
        auto v = m_levels[i]->get(key);
        if (!v) continue;

        V result = std::move(*v);

        if (i == 0)
        {
            m_last_hit_level = 0;
            m_hits[0]++;
            return result;
        }

        auto extracted = m_levels[i]->extract_ptr(key);
        if (!extracted) continue;

        std::optional<std::pair<K, std::unique_ptr<const V>>> moving = std::make_pair(key, std::move(extracted));

        for (std::size_t j = 0; j < m_levels.size() && moving.has_value(); j++)
        {
            auto evicted = m_levels[j]->insert_ptr(moving->first, std::move(moving->second));

            if (evicted) moving = std::move(evicted);
            else moving.reset();
        }

        m_last_hit_level = i;
        m_hits[i]++;

        return result;
    }

    V value = m_slow_get_page(key);

    auto vptr = std::make_unique<const V>(std::move(value));

    V result = *vptr;

    std::optional<std::pair<K, std::unique_ptr<const V>>> moving = std::make_pair(key, std::move(vptr));

    for (std::size_t j = 0; j < m_levels.size() && moving.has_value(); j++)
    {
        moving = m_levels[j]->insert_ptr(moving->first, std::move(moving->second));
    }

    return result;
    
}

#endif // CACHE_SYSTEM_TPP

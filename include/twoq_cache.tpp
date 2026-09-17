#ifndef TWOQ_CACHE_TPP
#define TWOQ_CACHE_TPP

template<typename K, typename V>
TwoQCache<K, V>::TwoQCache(std::size_t capacity)
    : Cache<K, V>(capacity),
    m_target_q1_size(std::max<std::size_t>(1, capacity / 4)),
    m_target_ghost_size(std::max<std::size_t>(1, capacity / 2)) {};



template<typename K, typename V>
void TwoQCache<K, V>::remove_from_list(Element_info& info)
{
    switch (info.list)
    {
        case List::Q1: m_q1.erase(info.it); break;
        case List::Q2: m_q2.erase(info.it); break;
        case List::GHOST: m_ghost.erase(info.it); break;
        case List::None: break;

    }

    info.list = List::None;
}

template<typename K, typename V>
void TwoQCache<K, V>::trim_ghost()
{
    while (m_q1.size() > m_target_q1_size)
    {
        m_element_infos.erase(m_q1.back());
        m_q1.pop_back();
    }
}

template<typename K, typename V>
std::optional<std::pair<K, V>> TwoQCache<K, V>::replace()
{
    std::optional<std::pair<K, V>> evicted;

    if (m_q1.size() + m_q2.size() >= m_capacity)
    {
        if (m_q1.size() > m_target_q1_size && !m_q1.empty())
        {
            K victim_key = m_q1.back();
            V victim_value = m_values[victim_key];
            m_q1.pop_back();

            Element_info& info = m_element_infos.at(victim_key);
            m_ghost.push_front(victim_key);

            info.it = m_ghost.begin();
            info.list = List::GHOST;
            info.is_resident = false;

            m_values.erase(victim_key);
            evicted = std::make_pair(victim_key, victim_value);
        }

        else if (!m_q2.empty()) // && m_q1.size() <= m_target_q1_size
        {
            K victim_key = m_q2.back();
            V victim_value = m_values[victim_key];
            m_q2.pop_back();

            m_element_infos.erase(victim_key);

            m_values.erase(victim_key);
            evicted = std::make_pair(victim_key, victim_value);


        }
    }

    trim_ghost();

    return evicted;
}






template<typename K, typename V>
bool TwoQCache<K, V>::contains(const K key) const
{
    auto it = m_element_infos.find(key);
    return it != m_element_infos.end() && it->second.is_resident;
}

template<typename K, typename V>
std::optional<V> TwoQCache<K, V>::get(const K key) const
{
    auto it = m_values.find(key);
    if (contains(key)) return it->second;

    return std::nullopt;
}




template<typename K, typename V>
std::optional<std::pair<K, V>> TwoQCache<K, V>::insert(const K key, const V value, bool is_user_request)
{
    auto it = m_element_infos.find(key);

    if (it != m_element_infos.end() && it->second.is_resident)
    {
        Element_info& info = it->second;
        remove_from_list(info);

        m_q2.push_front(key);
        info.it = m_q2.begin();

        info.list = List::Q2;
        m_values[key] = value;
        return std::nullopt;
    }

    std::optional<std::pair<K, V>> evicted = replace();

    if (it != m_element_infos.end() && it->second.list == List::GHOST)
    {
        Element_info& info = it->second;
        m_ghost.erase(info.it);

        m_q2.push_front(key);
        info.it = m_q2.begin();
        info.list = List::Q2;
        info.is_resident = true;
    }
    
    else
    {
        Element_info& info = m_element_infos.try_emplace(key).first->second;
        info.is_resident = true;

        m_q1.push_front(key);
        info.it = m_q1.begin();
        info.list = List::Q1;
    }

    m_values[key] = value;
    return evicted;
}



template<typename K, typename V>
void TwoQCache<K, V>::extract(const K key)
{
    auto it = m_element_infos.find(key);
    if (it == m_element_infos.end() || !it->second.is_resident)
    {
        return;
    }

    Element_info& info = it->second;

    remove_from_list(info);
    m_element_infos.erase(it);
    m_values.erase(key);
}




#endif // TWOQ_CACHE_TPP

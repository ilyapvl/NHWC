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
    while (m_ghost.size() > m_target_ghost_size)
    {
        m_element_infos.erase(m_ghost.back());
        m_ghost.pop_back();
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
    if (it != m_values.end()) return it->second;

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

    const bool was_in_ghost = (it != m_element_infos.end()) && (it->second.list == List::GHOST);

    bool target_q2 = was_in_ghost;
    auto saved = m_system_ghost.pop(key);

    if (saved.has_value())
    {
        if (saved->list == List::Q2) target_q2 = true;
    }

    std::optional<std::pair<K, V>> evicted = replace();


    it = m_element_infos.find(key);

    Element_info& info = (was_in_ghost && it != m_element_infos.end() && it->second.list == List::GHOST)
            ? it->second
            : m_element_infos.try_emplace(key).first->second;


    if (info.list == List::GHOST)
    {
        m_ghost.erase(info.it);
        info.list = List::None;
    }

    if (target_q2)
    {
        m_q2.push_front(key);

        info.it = m_q2.begin();
        info.list = List::Q2;
    }




    else
    {
        m_q1.push_front(key);

        info.it = m_q1.begin();
        info.list = List::Q1;
    }


    info.is_resident = true;

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

    m_system_ghost.save(key, State{info.list});
    remove_from_list(info);
    m_element_infos.erase(it);
    m_values.erase(key);
}




template<typename K, typename V>
void TwoQCache<K, V>::touch(const K key)
{
    auto it = m_element_infos.find(key);
    if (it == m_element_infos.end() || !it->second.is_resident) return;

    Element_info& info = it->second;
    remove_from_list(info);
    m_q2.push_front(key);

    info.it = m_q2.begin();
    info.list = List::Q2;
}







template<typename K, typename V>
void TwoQCache<K, V>::dump(std::ostream& out) const
{
    out << "TwoQCache (size=" << (m_q1.size() + m_q2.size())
        << "/" << m_capacity
        << ", |A1|=" << m_q1.size()
        << ", |Am|=" << m_q2.size()
        << ", |A1out|=" << m_ghost.size()
        << ", Kin=" << m_target_q1_size
        << ", Kout=" << m_target_ghost_size << ")\n";

    auto print_resident = [&](const std::list<K>& lst, const char* name) {
        out << "    " << name << " [MRU -> LRU]: ";
        bool first = true;
        for (const auto& key : lst) {
            if (!first) out << ", ";
            first = false;

            auto it = m_values.find(key);
            if (it != m_values.end()) out << key;
            else out << key << "=NOVALUE ";
        }
        out << '\n';
    };

    auto print_ghost = [&](const std::list<K>& lst, const char* name) {
        out << "    " << name << " [MRU -> LRU]: ";
        bool first = true;
        for (const auto& key : lst) {
            if (!first) out << ", ";
            first = false;
            out << key;
        }
        out << '\n';
    };

    print_resident(m_q1,    "A1 (recent)   ");
    print_resident(m_q2,    "Am (frequent) ");
    print_ghost   (m_ghost, "A1out (ghost) ");
}

#endif // TWOQ_CACHE_TPP

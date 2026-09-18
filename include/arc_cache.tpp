#ifndef ARC_CACHE_TPP
#define ARC_CACHE_TPP

template<typename K, typename V>
ARCCache<K, V>::ARCCache(std::size_t capacity) : Cache<K, V>(capacity), m_target_t1_size(0) {}




template<typename K, typename V>
void ARCCache<K, V>::remove_from_list(Element_info& element_info)
{
    switch (element_info.list)
    {
        case List::T1: m_t1.erase(element_info.it); break;
        case List::T2: m_t2.erase(element_info.it); break;
        case List::B1: m_b1.erase(element_info.it); break;
        case List::B2: m_b2.erase(element_info.it); break;

        case List::None: break;
    }

    element_info.list = List::None;
}

template<typename K, typename V>
void ARCCache<K, V>::push_to_front(std::list<K>& lst, const K& key, Element_info& element_info, List list_type)
{
    lst.push_front(key);
    element_info.it = lst.begin();
    element_info.list = list_type;
}

template<typename K, typename V>
std::optional<std::pair<K, V>> ARCCache<K, V>::replace(const bool hit_in_b2)
{
    const std::size_t current_t1_size = m_t1.size();

    const bool evict_from_t1 = (current_t1_size >= 1) && ((hit_in_b2 && current_t1_size == m_target_t1_size) || (current_t1_size > m_target_t1_size));

    K victim;
    V victim_value;

    if (evict_from_t1)
    {
        assert(!m_t1.empty());

        victim = m_t1.back();
        victim_value = m_values[victim];

        m_t1.pop_back();
        m_b1.push_front(victim);

        Element_info& element_info = m_element_infos.at(victim);

        element_info.it = m_b1.begin();
        element_info.list = List::B1;
        element_info.resident = false;
    }
    
    else
    {
        assert(!m_t2.empty());

        victim = m_t2.back();
        victim_value = m_values[victim];

        m_t2.pop_back();
        m_b2.push_front(victim);

        Element_info& element_info = m_element_infos.at(victim);

        element_info.it = m_b2.begin();
        element_info.list = List::B2;
        element_info.resident = false;
    }

    m_values.erase(victim);
    return std::make_pair(victim, victim_value);
}


template<typename K, typename V>
bool ARCCache<K, V>::contains(const K key) const
{
    auto it = m_element_infos.find(key);
    return it != m_element_infos.end() && it->second.resident;
}


template<typename K, typename V>
std::optional<V> ARCCache<K, V>::get(const K key) const
{
    auto it = m_values.find(key);
    if (it != m_values.end()) return it->second;

    return std::nullopt;
}

template<typename K, typename V>
std::optional<std::pair<K, V>> ARCCache<K, V>::insert(const K key, const V value, bool is_user_request)
{
    auto it = m_element_infos.find(key);

    // hit in T1 or T2
    // push into T2
    if (it != m_element_infos.end() && it->second.resident)
    {
        Element_info& element_info = it->second;

        remove_from_list(element_info);
        push_to_front(m_t2, key, element_info, List::T2);
        m_values[key] = value;

        return std::nullopt;
    }

    // hit in B1
    // t1 increase and erase resident
    if (it != m_element_infos.end() && it->second.list == List::B1)
    {
        const std::size_t b1_size = m_b1.size();
        const std::size_t b2_size = m_b2.size();
        const std::size_t delta = std::max(b2_size / b1_size, std::size_t(1));

        m_target_t1_size = std::min(m_target_t1_size + delta, m_capacity);

        std::optional<std::pair<K, V>> erased = replace(false);

        Element_info& element_info = it->second;
        remove_from_list(element_info);

        push_to_front(m_t2, key, element_info, List::T2);
        element_info.resident = true;
        m_values[key] = value;

        return erased;
    }

    // hit in b2
    // decrease t1 and erase resident
    if (it != m_element_infos.end() && it->second.list == List::B2)
    {
        const std::size_t b1_size = m_b1.size();
        const std::size_t b2_size = m_b2.size();
        const std::size_t delta = std::max(b1_size / b2_size, std::size_t(1));

        m_target_t1_size = (m_target_t1_size >= delta) ? (m_target_t1_size - delta) : 0;

        std::optional<std::pair<K, V>> erased = replace(true);

        Element_info& element_info = it->second;
        remove_from_list(element_info);

        push_to_front(m_t2, key, element_info, List::T2);

        element_info.resident = true;
        m_values[key] = value;

        return erased;
    }

    // miss
    std::optional<std::pair<K, V>> erased;
    const std::size_t current_t1_size = m_t1.size();
    const std::size_t b1_size = m_b1.size();
    const std::size_t t2_size = m_t2.size();
    const std::size_t b2_size = m_b2.size();

    List target_list = List::T1;
    auto saved = m_system_ghost.pop(key);
    if (saved.has_value())
    {
        target_list = saved->list;
    }

    if (current_t1_size + b1_size == m_capacity)
    {
        if (current_t1_size < m_capacity)
        {
            K b1_victim = m_b1.back();
            m_b1.pop_back();
            m_element_infos.erase(b1_victim);

            erased = replace(false);
        }
        
        else
        {
            // b1 is full
            K t1_victim = m_t1.back();
            V t1_value = m_values[t1_victim];

            m_t1.pop_back();
            m_values.erase(t1_victim);
            m_element_infos.erase(t1_victim);
            erased = std::make_pair(t1_victim, t1_value);
        }
    }


    else // t1 + b1 < capacity
    {
        const std::size_t total = current_t1_size + t2_size + b1_size + b2_size;

        if (total >= m_capacity)
        {
            if (total == 2 * m_capacity)
            {
                K b2_victim = m_b2.back();
                m_b2.pop_back();
                m_element_infos.erase(b2_victim);
            }


            erased = replace(false);
        }
    }

    Element_info& element_info = m_element_infos.try_emplace(key).first->second;
    element_info.resident = true;
    
    if (target_list == List::T2) //TODO - move this into push_to_front
        push_to_front(m_t2, key, element_info, List::T2);
    else
        push_to_front(m_t1, key, element_info, List::T1);

    m_values[key] = value;

    return erased;
}

template<typename K, typename V>
void ARCCache<K, V>::extract(const K key)
{
    auto it = m_element_infos.find(key);
    if (it == m_element_infos.end() || !it->second.resident)
    {
        return;
    }

    Element_info& element_info = it->second;

    m_system_ghost.save(key, State{element_info.list});

    remove_from_list(element_info);
    m_element_infos.erase(it);
    m_values.erase(key);
}




template<typename K, typename V>
void ARCCache<K, V>::touch(const K key)
{
    auto it = m_element_infos.find(key);
    if (it == m_element_infos.end() || !it->second.resident) return;

    Element_info& info = it->second;
    remove_from_list(info);
    push_to_front(m_t2, key, info, List::T2);
}




template<typename K, typename V>
void ARCCache<K, V>::dump(std::ostream& out) const
{
    out << "ARCCache (size=" << (m_t1.size() + m_t2.size())
        << "/" << m_capacity
        << ", |T1|=" << m_t1.size()
        << ", |T2|=" << m_t2.size()
        << ", |B1|=" << m_b1.size()
        << ", |B2|=" << m_b2.size()
        << ", p=" << m_target_t1_size << ")\n";

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

    print_resident(m_t1, "T1 (recent)  ");
    print_resident(m_t2, "T2 (frequent)");
    print_ghost   (m_b1, "B1 (ghost T1)");
    print_ghost   (m_b2, "B2 (ghost T2)");
}

#endif // ARC_CACHE_TPP

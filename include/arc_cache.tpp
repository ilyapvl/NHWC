#ifndef ARC_CACHE_TPP
#define ARC_CACHE_TPP

namespace cache
{

template<typename K, typename V>
ARCCache<K, V>::ARCCache(std::size_t capacity, std::size_t ghost_capacity)
    : Cache<K, V>(capacity),
    m_target_t1_size(0.0),
    m_system_ghost(ghost_capacity) {}




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
std::optional<std::pair<K, std::unique_ptr<const V>>> ARCCache<K, V>::replace(const bool hit_in_b2)
{
    const std::size_t current_t1_size = m_t1.size();

    if (m_t1.empty() && m_t2.empty()) return std::nullopt;

    const bool evict_from_t1 =
        (current_t1_size > 0) &&
        ((current_t1_size > m_target_t1_size) ||
        (current_t1_size == m_target_t1_size && hit_in_b2));

    

    K victim;
    std::unique_ptr<const V> vptr;

    if (evict_from_t1 || m_t2.empty())
    {
        assert(!m_t1.empty());

        victim = m_t1.back();

        vptr = std::move(m_values[victim]);

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
        vptr = std::move(m_values[victim]);

        m_t2.pop_back();
        m_b2.push_front(victim);

        Element_info& element_info = m_element_infos.at(victim);

        element_info.it = m_b2.begin();
        element_info.list = List::B2;
        element_info.resident = false;
    }

    m_values.erase(victim);
    return std::make_pair(victim, std::move(vptr));
}


template<typename K, typename V>
bool ARCCache<K, V>::contains(const K& key) const
{
    auto it = m_element_infos.find(key);
    return it != m_element_infos.end() && it->second.resident;
}


template<typename K, typename V>
std::optional<V> ARCCache<K, V>::get(const K& key)
{
    auto it = m_element_infos.find(key);
    if (it == m_element_infos.end() || !it->second.resident)
    {
        return std::nullopt;
    }

    Element_info& info = it->second;
    
    remove_from_list(info);
    push_to_front(m_t2, key, info, List::T2);

    auto vit = m_values.find(key);
    if (vit == m_values.end() || !vit->second)
    {
        return std::nullopt;
    }

    return *vit->second;
}

template<typename K, typename V>
std::optional<std::pair<K, std::unique_ptr<const V>>> ARCCache<K, V>::insert_ptr(const K& key, std::unique_ptr<const V> vptr)
{
    if (!vptr) return std::nullopt;

    auto it = m_element_infos.find(key);
    const std::size_t resident_size = m_t1.size() + m_t2.size();

    // hit in T1 or T2
    // push into T2
    if (it != m_element_infos.end() && it->second.resident)
    {
        Element_info& info = it->second;
        remove_from_list(info);
        push_to_front(m_t2, key, info, List::T2);
        m_values[key] = std::move(vptr);
        return std::nullopt;
    }

    // hit in B1
    // t1 increase and erase resident
    if (it != m_element_infos.end() && it->second.list == List::B1)
    {
        const std::size_t b1 = m_b1.size();
        const std::size_t b2 = m_b2.size();
        const double delta = std::max(static_cast<double>(b2) / static_cast<double>(b1), 1.0);
        m_target_t1_size = std::min(m_target_t1_size + delta, static_cast<double>(m_capacity));

        std::optional<std::pair<K, std::unique_ptr<const V>>> erased;
        if (resident_size == m_capacity) erased = replace(false);

        Element_info& info = it->second;
        remove_from_list(info);
        push_to_front(m_t2, key, info, List::T2);
        info.resident = true;
        m_values[key] = std::move(vptr);
        return erased;
    }

    // hit in b2
    // decrease t1 and erase resident
    if (it != m_element_infos.end() && it->second.list == List::B2)
    {
        const std::size_t b1 = m_b1.size();
        const std::size_t b2 = m_b2.size();
        const double delta = std::max(static_cast<double>(b1) / static_cast<double>(b2), 1.0);
        m_target_t1_size = std::max(m_target_t1_size - delta, 0.0);

        std::optional<std::pair<K, std::unique_ptr<const V>>> erased;
        if (resident_size == m_capacity) erased = replace(true);

        Element_info& info = it->second;
        remove_from_list(info);
        push_to_front(m_t2, key, info, List::T2);
        info.resident = true;
        m_values[key] = std::move(vptr);
        return erased;
    }

    // miss
    std::optional<std::pair<K, std::unique_ptr<const V>>> erased;

    // invariants-based eviction
    // [A] T1 + B1 == cap
    if (m_t1.size() + m_b1.size() == m_capacity)
    {
        if (!m_b1.empty())
        {
            // trim LRU из B1
            K victim = m_b1.back();
            m_b1.pop_back();
            m_element_infos.erase(victim);
        }

        else if (!m_t1.empty())
        {
            // B1 == 0, T1 == cap: evict T1
            K victim = m_t1.back();

            auto vit = m_values.find(victim);
            if (vit != m_values.end())
            {
                auto vptr2 = std::move(vit->second);
                m_values.erase(vit);
                m_t1.pop_back();
                m_element_infos.erase(victim);

                if (!erased) erased = std::make_pair(victim, std::move(vptr2));
            }
        }
    }

    // [B] total == 2*cap
    if (m_t1.size() + m_t2.size() + m_b1.size() + m_b2.size() == 2 * m_capacity)
    {
        if (!m_b2.empty())
        {
            K victim = m_b2.back();
            m_b2.pop_back();
            m_element_infos.erase(victim);
        }
    }

    // [C] resident == cap
    // replace
    if (m_t1.size() + m_t2.size() == m_capacity)
    {
        auto e = replace(false);
        if (e && !erased) erased = std::move(e);
    }

    // insert into T1
    Element_info& info = m_element_infos.try_emplace(key).first->second;
    info.resident = true;
    push_to_front(m_t1, key, info, List::T1);
    m_values[key] = std::move(vptr);

    return erased;
}


template<typename K, typename V>
std::unique_ptr<const V> ARCCache<K, V>::extract_ptr(const K& key)
{
    auto it = m_element_infos.find(key);
    if (it == m_element_infos.end() || !it->second.resident)
    {
        return nullptr;
    }

    Element_info& element_info = it->second;

    m_system_ghost.save(key, State{element_info.list});

    auto vit = m_values.find(key);
    auto value = (vit != m_values.end()) ? std::move(vit->second) : nullptr;

    remove_from_list(element_info);
    m_element_infos.erase(it);
    m_values.erase(key);

    return value;
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


}

#endif // ARC_CACHE_TPP

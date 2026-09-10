#ifndef LIRS_CACHE_TPP
#define LIRS_CACHE_TPP

template<typename K, typename V>
LIRSCache<K, V>::LIRSCache(std::size_t capacity)
    : Cache<K, V>(capacity),
    m_lir_capacity(get_lir_capacity(capacity)),
    m_resident_count(0),
    m_lir_count(0) {}

template<typename K, typename V>
std::size_t LIRSCache<K, V>::get_lir_capacity(std::size_t capacity)
{
    if (capacity < 2) return 0;

    return capacity - std::max<std::size_t>(1, capacity / 100);
}

template<typename K, typename V>
void LIRSCache<K, V>::move_to_stack_front(const K key, Element_info& element_info)
{
    if (element_info.in_stack)
    {
        m_stack.splice(m_stack.begin(), m_stack, element_info.stack_it);
    }

    else
    {
        m_stack.push_front(key);

        element_info.stack_it = m_stack.begin();
        element_info.in_stack = true;
    }
}

template<typename K, typename V>
void LIRSCache<K, V>::move_to_queue_front(const K key, Element_info& element_info)
{
    assert(element_info.resident);
    assert(!element_info.lir);

    if (element_info.in_queue)
    {
        m_queue.splice(m_queue.begin(), m_queue, element_info.queue_it);
    }

    else
    {
        m_queue.push_front(key);
        element_info.queue_it = m_queue.begin();
        element_info.in_queue = true;
    }
}


template<typename K, typename V>
void LIRSCache<K, V>::remove_from_queue(Element_info& element_info)
{
    if (element_info.in_queue)
    {
        m_queue.erase(element_info.queue_it);
        element_info.in_queue = false;
    }
}

template<typename K, typename V>
void LIRSCache<K, V>::remove_from_stack(Element_info& element_info)
{
    if (element_info.in_stack)
    {
        m_stack.erase(element_info.stack_it);
        element_info.in_stack = false;
    }
}


template<typename K, typename V>
void LIRSCache<K, V>::remove_hir_from_stack_bottom()
{
    while (!m_stack.empty())
    {
        auto it = m_element_infos.find(m_stack.back());
        Element_info& element_info = it->second;

        if (element_info.lir) break;

        m_stack.pop_back();

        element_info.in_stack = false;

        if (!element_info.resident)
        {
            m_element_infos.erase(it);
        }
    }
}


template<typename K, typename V>
void LIRSCache<K, V>::exctract_hir()
{
    K element_to_extract = m_queue.back();

    auto it = m_element_infos.find(element_to_extract);

    Element_info& element_info = it->second;

    remove_from_queue(element_info);

    element_info.resident = false;

    m_resident_count--;

    if (!element_info.in_stack)
    {
        m_element_infos.erase(it);
    }
}


template<typename K, typename V>
void LIRSCache<K, V>::bottom_lir_to_hir()
{
    remove_hir_from_stack_bottom();

    if (m_stack.empty()) return;

    const K key = m_stack.back();

    Element_info& info = m_element_infos.at(key);

    info.lir = false;

    m_lir_count--;

    move_to_queue_front(key, info);
}


template<typename K, typename V>
void LIRSCache<K, V>::hir_to_lir(Element_info& info)
{
    remove_from_queue(info);
    info.lir = true;
    m_lir_count++;

    if (m_lir_count > m_lir_capacity)
    {
        bottom_lir_to_hir();
    }
}



template<typename K, typename V>
void LIRSCache<K, V>::restore_lir_after_extraction()
{
    const std::size_t desired = std::min(m_lir_capacity, m_resident_count);

    while (m_lir_count < desired)
    {
        if (m_queue.empty()) break;
        const K key = m_queue.front();
        Element_info& info = m_element_infos.at(key);

        remove_from_queue(info);
        move_to_stack_front(key, info);

        info.lir = true;
        ++m_lir_count;
    }

    remove_hir_from_stack_bottom();
}







template<typename K, typename V>
bool LIRSCache<K, V>::contains(const K key) const
{
    auto it = m_element_infos.find(key);

    return it != m_element_infos.end() && it->second.resident;
}

template<typename K, typename V>
std::optional<V> LIRSCache<K, V>::get(const K key) const
{
    auto it = m_values.find(key);
    if (it != m_values.end())
    {
        return it->second;
    }

    return std::nullopt;
}

template<typename K, typename V>
std::optional<std::pair<K, V>> LIRSCache<K, V>::insert(const K key, const V value, const bool is_user_request)
{
    if (contains(key))
    {
        Element_info& info = m_element_infos.at(key);
        if (info.lir)
        {
            move_to_stack_front(key, info);
        }
        
        else
        {
            const bool was_in_stack = info.in_stack;
            move_to_stack_front(key, info);
            if (was_in_stack && m_lir_capacity > 0)
            {
                hir_to_lir(info);
            }
            
            else
            {
                move_to_queue_front(key, info);
            }
        }
        
        m_values[key] = value;


        return std::nullopt;
    }

    if (m_capacity == 0)
    {
        return std::make_pair(key, value);
    }

    auto old = m_element_infos.find(key);
    const bool was_in_stack = (old != m_element_infos.end() && old->second.in_stack);

    std::optional<std::pair<K, V>> erased_element;

    if (m_resident_count == m_capacity)
    {
        K erased_key = m_queue.back();
        V erased_value = m_values[erased_key];
        m_values.erase(erased_key);
        exctract_hir();
        erased_element = std::make_pair(erased_key, erased_value);
    }

    Element_info& info = m_element_infos.try_emplace(key).first->second;
    assert(!info.resident && !info.lir);

    info.resident = true;
    m_resident_count++;

    move_to_stack_front(key, info);

    const bool warmup = m_lir_count < m_lir_capacity;
    const bool repeated_request = was_in_stack && m_lir_capacity > 0 && is_user_request;

    if (warmup || repeated_request)
    {
        hir_to_lir(info);
    }
    
    else
    {
        move_to_queue_front(key, info);
    }


    m_values[key] = value;

    return erased_element;
}

template<typename K, typename V>
void LIRSCache<K, V>::extract(const K key)
{
    auto it = m_element_infos.find(key);
    if (it == m_element_infos.end() || !it->second.resident) return;

    Element_info& info = it->second;
    V value = m_values[key];

    if (info.lir)
    {
        m_lir_count--;
    }
    m_resident_count--;

    remove_from_queue(info);
    remove_from_stack(info);
    m_element_infos.erase(it);
    m_values.erase(key);

    restore_lir_after_extraction();

}

#endif // LIRS_CACHE_TPP

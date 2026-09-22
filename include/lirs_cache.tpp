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

    return capacity - std::max<std::size_t>(MIN_HIR, capacity / LIR_HIR_RATIO);
}

template<typename K, typename V>
void LIRSCache<K, V>::move_to_stack_front(const K& key, Element_info& element_info)
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
void LIRSCache<K, V>::move_to_queue_front(const K& key, Element_info& element_info)
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

    const K& key = m_stack.back();

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
std::optional<K> LIRSCache<K, V>::restore_lir_after_extraction()
{
    const std::size_t desired = std::min(m_lir_capacity, m_resident_count);
    std::optional<K> promoted;

    while (m_lir_count < desired)
    {
        if (m_queue.empty()) break;
        const K key = m_queue.front();
        Element_info& info = m_element_infos.at(key);

        remove_from_queue(info);
        move_to_stack_front(key, info);

        info.lir = true;
        ++m_lir_count;

        promoted = key;
    }

    remove_hir_from_stack_bottom();

    return promoted;
}


template<typename K, typename V>
void LIRSCache<K, V>::cut_stack()
{
    const std::size_t high_size = 3 * m_capacity;
    const std::size_t low_size = 2 * m_capacity;

    if (m_stack.size() <= high_size) return;

    auto it = m_stack.end();
    while (it != m_stack.begin() && m_stack.size() > low_size)
    {
        it--;

        auto info_it = m_element_infos.find(*it);

        if (info_it->second.resident) continue;

        K key = *it;
        it = m_stack.erase(it);
        m_element_infos.erase(key);
    }
}












template<typename K, typename V>
bool LIRSCache<K, V>::contains(const K& key) const
{
    auto it = m_element_infos.find(key);

    return it != m_element_infos.end() && it->second.resident;
}

template<typename K, typename V>
std::optional<V> LIRSCache<K, V>::get(const K& key)
{
    auto it = m_element_infos.find(key);
    if (it == m_element_infos.end() || !it->second.resident) return std::nullopt;

    Element_info& info = it->second;

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

    remove_hir_from_stack_bottom();
    cut_stack();

    auto vit = m_values.find(key);
    if (vit == m_values.end() || !vit->second) return std::nullopt;

    return *vit->second;
}

template<typename K, typename V>
std::optional<std::pair<K, std::unique_ptr<const V>>> LIRSCache<K, V>::insert_ptr(const K& key, std::unique_ptr<const V> vptr)
{
    if (!vptr) return std::nullopt;
    if (contains(key))
    {
        m_values[key] = std::move(vptr);

        return std::nullopt;
    }

    auto saved = m_system_ghost.pop(key);
    const bool ghost_promotes_to_lir = saved.has_value() && saved->is_lir;


    if (ghost_promotes_to_lir && m_last_promoted.has_value())
    {
        K last_promoted = m_last_promoted.value();
        auto it = m_element_infos.find(last_promoted);

        if (it != m_element_infos.end() && it->second.resident && it->second.lir)
        {
            Element_info& last_promoted_info = it->second;
            last_promoted_info.lir = false;
            m_lir_count--;
            move_to_queue_front(last_promoted, last_promoted_info);
        }
    }

    m_last_promoted.reset();




    auto old = m_element_infos.find(key);
    const bool was_in_stack = (old != m_element_infos.end()) && old->second.in_stack;

    std::optional<std::pair<K, std::unique_ptr<const V>>> erased;

    if (m_resident_count == m_capacity)
    {
        K erased_key = m_queue.back();
        auto vit = m_values.find(erased_key);

        auto erased_vptr = std::move(vit->second);
        m_values.erase(vit);

        exctract_hir();
        erased = std::make_pair(erased_key, std::move(erased_vptr));
    }

    Element_info& info = m_element_infos.try_emplace(key).first->second;
    assert(!info.resident && !info.lir);

    info.resident = true;
    m_resident_count++;

    move_to_stack_front(key, info);

    const bool warmup = m_lir_count < m_lir_capacity;
    const bool repeated_request = was_in_stack && m_lir_capacity > 0;

    if (warmup || repeated_request || ghost_promotes_to_lir)
    {
        hir_to_lir(info);
    }
    
    else
    {
        move_to_queue_front(key, info);
    }


    m_values[key] = std::move(vptr);


    cut_stack();

    return erased;
}

template<typename K, typename V>
std::unique_ptr<const V> LIRSCache<K, V>::extract_ptr(const K& key)
{
    auto it = m_element_infos.find(key);
    if (it == m_element_infos.end() || !it->second.resident) return nullptr;

    Element_info& info = it->second;

    m_system_ghost.save(key, State{info.lir});

    auto vit = m_values.find(key);
    auto vptr = (vit != m_values.end()) ? std::move(vit->second) : nullptr;


    if (info.lir)
    {
        m_lir_count--;
    }
    m_resident_count--;

    remove_from_queue(info);
    remove_from_stack(info);
    m_element_infos.erase(it);
    m_values.erase(key);

    m_last_promoted.reset();
    m_last_promoted = restore_lir_after_extraction();

    cut_stack();

    return vptr;
}



template<typename K, typename V>
void LIRSCache<K, V>::dump(std::ostream& out) const
{
    std::size_t ghost_count = 0;
    for (const auto& [_, info] : m_element_infos) {
        if (!info.resident) ++ghost_count;
    }

    out << "LIRSCache (resident=" << m_resident_count << "/" << this->m_capacity
        << ", LIR=" << m_lir_count << "/" << m_lir_capacity
        << ", HIR=" << m_queue.size()
        << ", GHOST=" << ghost_count << ")\n";

    out << "    S [top -> bottom]: ";
    bool first = true;
    for (const auto& key : m_stack) {
        if (!first) out << ", ";
        first = false;

        auto it = m_element_infos.find(key);
        if (it == m_element_infos.end()) {
            out << key << ":?";
            continue;
        }

        const Element_info& info = it->second;
        const char* state =
            info.lir      ? "LIR"   :
            info.resident ? "HIR"   : "GHOST";

        out << key << ":" << state;
    }
    out << '\n';

    out << "    Q [MRU -> LRU]:     ";
    first = true;
    for (const auto& key : m_queue) {
        if (!first) out << ", ";
        first = false;

        auto vit = m_values.find(key);
        if (vit != m_values.end())
            out << key;
        else
            out << key << "=NOVALUE ";
    }
    out << '\n';
}

#endif // LIRS_CACHE_TPP

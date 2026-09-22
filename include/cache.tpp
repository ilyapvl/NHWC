#ifndef CACHE_TPP
#define CACHE_TPP

template <typename K, typename V>
Cache<K, V>::Cache(std::size_t capacity) : m_capacity(capacity) {}

template <typename K, typename V>
std::size_t Cache<K, V>::capacity() const
{
    return m_capacity;
}

#endif // CACHE_TPP

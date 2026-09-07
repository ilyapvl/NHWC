#include <iostream>
#include <optional>
#include <list>
#include <unordered_map>
#include <cassert>



template <typename K, typename V>
class Cache
{
protected:
    std::size_t m_capacity;

public:
    Cache(std::size_t capacity) : m_capacity(capacity) {}
    virtual ~Cache() = default;

    std::size_t capacity() const
    {
        return m_capacity;
    }

    virtual bool contains(const K key) const = 0;
    virtual void key_found_to_front(const K key) = 0;

    virtual void extract(const K key) = 0;

    

    //virtual void dump(std::ostream& out) const = 0;
};

template<typename K, typename V>
class LRUCache : public Cache<K, V>
{
    
private:
    std::list<K> m_order;
    std::unordered_map<K, typename std::list<K>::iterator> m_positions;



public:
    LRUCache(std::size_t capacity) : Cache<K, V>(capacity) {}
    using Cache<K, V>::m_capacity;

    bool contains(const K key) const override
    {
        return m_positions.find(key) != m_positions.end();
    }

    void key_found_to_front(const K key) override
    {
        const auto old_position = m_positions.at(key);
        m_order.splice(m_order.begin(), m_order, old_position);
    }

    void extract(const K key) override
    {
        auto it = m_positions.find(key);

        assert(it != m_positions.end());

        m_order.erase(it->second);
        m_positions.erase(it);

    }


    std::optional<K> insert(const K key)
    {
        if (m_capacity == 0) return key;

        std::optional<K> element_to_erase;

        if (m_order.size() == m_capacity)
        {
            element_to_erase = m_order.back();
            m_positions.erase(*element_to_erase);
            m_order.pop_back();

        }

        m_order.push_front(key);
        m_positions.emplace(key, m_order.begin());

        return element_to_erase;
    }


};


int main()
{
    LRUCache<int, int> cache(2);

    cache.insert(5);
    cache.insert(4);
    cache.insert(3);
    std::cout << cache.contains(5);
    std::cout << cache.contains(4);

    return 0;
}

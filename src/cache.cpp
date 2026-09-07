#include <iostream>
#include <optional>
#include <list>
#include <unordered_map>
#include <cassert>
#include <memory>
#include <string>



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
    virtual std::optional<K> insert(const K key) = 0;
    

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


    std::optional<K> insert(const K key) override
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

template<typename K, typename V>
std::unique_ptr<Cache<K, V>> make_cache(const std::string& algorithm, size_t capacity)
{
    if (algorithm == "LRU")
    {
        return std::make_unique<LRUCache<K, V>>(capacity);
    }

    assert(false);
}




class HitCounter
{
private:
    size_t m_hits = 0;

public:
    void record_hit() { ++m_hits; }
    size_t hits() { return m_hits; }


};




template<typename K, typename V>
class CacheSystem
{
private:
    std::vector<std::unique_ptr<Cache<K, V>>> m_levels;
    HitCounter& m_hit_counter;

public:
    CacheSystem(std::vector<std::unique_ptr<Cache<K, V>>> levels, HitCounter& hit_counter)
        : m_levels(std::move(levels)), m_hit_counter(hit_counter)
    {

    }


    std::optional<size_t> access(const K key)
    {
        std::optional<size_t> hit_level;

        for (size_t i = 0; i < m_levels.size(); i++)
        {
            if (m_levels[i]->contains(key))
            {
                hit_level = i;
                
                break;
            }
        }

        if (hit_level.has_value())
        {
            m_hit_counter.record_hit();

            if (*hit_level == 0)
            {
                m_levels[*hit_level]->key_found_to_front(key);

                return hit_level;
            }

            m_levels[*hit_level]->extract(key);
        }


        std::optional moving_key = key;

        for (size_t i = 0; i < m_levels.size() && moving_key.has_value(); i++)
        {
            moving_key = m_levels[i]->insert(*moving_key);
        }

        return hit_level;
        
    }


};


int main()
{
    HitCounter cntr;

    std::vector<std::unique_ptr<Cache<int, int>>> levels;
    levels.reserve(3);

    for (std::size_t i = 0; i < 3; ++i)
    {
        

        levels.push_back(make_cache<int, int>("LRU", 5));
    }

    CacheSystem<int, int> chs(std::move(levels), cntr);

    chs.access(1);
    chs.access(1);
    chs.access(1);
    chs.access(2);
    chs.access(4);

    std::cout << cntr.hits();

    return 0;
}

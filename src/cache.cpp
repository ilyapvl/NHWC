#include <iostream>
#include <optional>
#include <list>
#include <unordered_map>
#include <map>
#include <cassert>
#include <memory>
#include <string>
#include <limits>



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

        auto it = m_positions.find(key);

        if (it != m_positions.end())
        {
            m_order.splice(m_order.begin(), m_order, it->second);

            return std::nullopt;
        }

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
class LFUCache : public Cache<K, V>
{
private:
    std::map<int, std::list<K>> m_freq_to_keys;
    std::unordered_map<K, std::pair<int, typename std::list<K>::iterator>> m_key_to_pair;

    void increment_frequency(const K key)
    {
        auto& [freq, it] = m_key_to_pair[key];

        auto& old_list = m_freq_to_keys[freq];
        old_list.erase(it);

        if (old_list.empty()) m_freq_to_keys.erase(freq);

        freq++;

        auto& new_list = m_freq_to_keys[freq];

        new_list.push_front(key);
        it = new_list.begin();

    }

public:
    LFUCache(size_t capacity) : Cache<K, V>(capacity) {};
    using Cache<K, V>::m_capacity;

    bool contains(const K key) const override
    {
        return m_key_to_pair.find(key) != m_key_to_pair.end();
    }

    void extract(const K key) override
    {
        auto it = m_key_to_pair.find(key);
        if (it == m_key_to_pair.end()) return;

        int freq = it->second.first;

        auto list_it = it->second.second;
        m_freq_to_keys[freq].erase(list_it);

        if (m_freq_to_keys[freq].empty())
        {
            m_freq_to_keys.erase(freq);
        }

        m_key_to_pair.erase(it);
    }


    std::optional<K> insert(const K key) override
    {
        if (contains(key))
        {
            increment_frequency(key);

            return std::nullopt;
        }

        std::optional<K> element_to_erase;

        if (m_key_to_pair.size() == m_capacity)
        {
            auto& min_freq_list = m_freq_to_keys.begin()->second;

            K victim = min_freq_list.back();
            min_freq_list.pop_back();

            if (min_freq_list.empty())
            {
                m_freq_to_keys.erase(m_freq_to_keys.begin());
            }


            m_key_to_pair.erase(victim);
            element_to_erase = victim;
        }


        m_freq_to_keys[1].push_front(key);

        m_key_to_pair.emplace(key, std::make_pair(1, m_freq_to_keys[1].begin()));

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

    else if (algorithm == "LFU")
    {
        return std::make_unique<LFUCache<K, V>>(capacity);
    }

    assert(false);
}






template<typename K, typename V>
class CacheSystem
{
private:
    std::vector<std::unique_ptr<Cache<K, V>>> m_levels;

public:
    CacheSystem(std::vector<std::unique_ptr<Cache<K, V>>> levels)
        : m_levels(std::move(levels))
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

            if (*hit_level == 0)
            {
                m_levels[*hit_level]->insert(key);
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

template<typename K, typename V>
void access_and_record_hit(K key, CacheSystem<K, V>& chs, std::vector<int>& hits)
{
    auto hit_level = chs.access(key);

    if (hit_level.has_value())
    {
        hits[hit_level.value()]++;
    }

}


int main()
{

    std::vector<std::unique_ptr<Cache<int, int>>> levels;
    levels.reserve(3);

    levels.push_back(make_cache<int, int>("LRU", 2));
    levels.push_back(make_cache<int, int>("LFU", 5));
    

    CacheSystem<int, int> chs(std::move(levels));

    std::vector<int> hits = {};
    hits.reserve(3);


    access_and_record_hit(5, chs, hits);
    access_and_record_hit(6, chs, hits);
    access_and_record_hit(5, chs, hits);
    access_and_record_hit(7, chs, hits);
    access_and_record_hit(5, chs, hits);
    access_and_record_hit(6, chs, hits);


    for (int i = 0; i < 3; i++)
    {
        std::cout << "level " << i << " has " << hits[i] << " hits\n";
    }

    return 0;
}

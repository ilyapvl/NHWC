#include "cache_system.h"
#include <iostream>
#include <vector>
#include <memory>

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

#include "cache_system.h"
#include <iostream>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <string>

template<typename K, typename V>
void access_and_record_hit(K key, CacheSystem<K, V>& chs, std::vector<int>& hits)
{
    auto hit_level = chs.access(key);

    if (hit_level.has_value())
    {
        hits[hit_level.value()]++;
    }
}

int main(const int argc, const char* argv[])
{
    if (argc < 2)
    {
        std::cout << "No file specified" << std::endl;

        return 1;
    }
    
    std::ifstream config(argv[1]);
    if (!config.is_open())
    {
        std::cerr << "Failed to open file" << std::endl;

        return 1;
    }

    int level_count;

    if (!(config >> level_count))
    {
        std::cerr << "Failed to read level count" << std::endl;

        return 1;
    }


    std::vector<std::string> algorithms(level_count);
    for (int i = 0; i < level_count; i++)
    {
        if (!(config >> algorithms[i]))
        {
            std::cerr << "Failed to read algorithms" << std::endl;
            return 1;
        }
    }

    std::vector<int> capacities(level_count);
    for (int i = 0; i < level_count; i++)
    {
        if (!(config >> capacities[i]))
        {
            std::cerr << "Failed to read capacities" << std::endl;
            return 1;
        }
    }



    std::vector<std::unique_ptr<Cache<int, int>>> levels;
    levels.reserve(level_count);
    for (int i = 0; i < level_count; i++)
    {
        levels.push_back(make_cache<int, int>(algorithms[i], capacities[i]));
    }


    int num_requests = 0;
    if (!(config >> num_requests))
    {
        std::cerr << "Failed to read requests count" << std::endl;

        return 1;
    }

    CacheSystem<int, int> chs(std::move(levels));

    std::vector<int> hits(level_count, 0);


    int key;
    while (config >> key)
    {
        access_and_record_hit(key, chs, hits);
    }


    for (int i = 0; i < level_count; i++)
    {
        std::cout << "level " << i << " " << algorithms[i] << " has " << hits[i] << " hits\n";
    }

    int total_hits = 0;

    for (int i = 0; i < level_count; i++) total_hits += hits[i];

    std::cout << "total " << total_hits << " / " << num_requests << std::endl;

    return 0;
}

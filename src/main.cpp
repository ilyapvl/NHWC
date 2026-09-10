#include "cache_system.h"
#include "opt_cache.h"
#include <iostream>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <string>
#include <cstddef>

int slow_get_page(int key)
{
    // for (int i = 0; i < 1000000000; i += 2) i--;
    return key / 2;
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

    CacheSystem<int, int> chs(std::move(levels), slow_get_page);

    std::vector<int> hits(level_count, 0);

    std::vector<int> requests(num_requests, 0);
    
    for (int i = 0; i < num_requests; i++)
    {
        config >> requests[i];

        chs.access(requests[i]);
    }


    for (int i = 0; i < level_count; i++)
    {
        std::cout << "level " << i << " " << algorithms[i] 
        << " size " << capacities[i] << " has " << chs.get_hits(i) << " hits\n";
    }

    int total_hits = 0;

    for (int i = 0; i < level_count; i++) total_hits += chs.get_hits(i);

    std::cout << "total " << total_hits << " / " << num_requests << std::endl;



    std::size_t total_capacity = 0;
    for (int c : capacities) total_capacity += c;

    OptimalCache<int, int> ideal(total_capacity, requests);
    int ideal_hits = ideal.simulate();

    std::cout << "ideal: " << ideal_hits << " / " << num_requests << std::endl;

    return 0;
}

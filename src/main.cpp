#include "cache_system.h"
#include "lirs_cache.h"
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


bool read_config(const std::string filename, int& level_count,
    std::vector<std::string>& algorithms, std::vector<std::size_t>& capacities,
    int& num_requests, std::vector<int>& requests)
{
    std::ifstream config(filename);
    if (!config.is_open())
    {
        std::cerr << "Failed to open file" << std::endl;

        return 1;
    }

    if (!(config >> level_count))
    {
        std::cerr << "Failed to read level count" << std::endl;

        return 1;
    }


    algorithms.resize(level_count);
    for (int i = 0; i < level_count; i++)
    {
        if (!(config >> algorithms[i]))
        {
            std::cerr << "Failed to read algorithms" << std::endl;
            return 1;
        }
    }

    capacities.resize(level_count);
    for (int i = 0; i < level_count; i++)
    {
        if (!(config >> capacities[i]))
        {
            std::cerr << "Failed to read capacities" << std::endl;
            return false;
        }
    }

    if (!(config >> num_requests))
    {
        std::cerr << "Failed to read requests count" << std::endl;

        return false;
    }

    requests.resize(num_requests);
    for (int i = 0; i < num_requests; i++)
    {
        if (!(config >> requests[i]))
        {
            std::cerr << "Failed to read requests" << std::endl;
            return false;
        }
    }

    return true;
}

int main(const int argc, const char* argv[])
{
    if (argc < 2)
    {
        std::cout << "No file specified" << std::endl;

        return 1;
    }














    int level_count = 0;
    std::vector<std::string> algorithms = {};
    std::vector<std::size_t> capacities = {};
    std::vector<std::unique_ptr<Cache<int, int>>> levels = {};
    int num_requests = {};
    std::vector<int> requests = {};

    if(!read_config(argv[1], level_count, algorithms, capacities, num_requests, requests)) return 1;

    



    CacheSystem<int, int> chs(level_count, slow_get_page);
    int chs_err = 0;

    for (int i = 0; i < level_count; i++)
    {
        chs_err = chs.add_cache(algorithms[i], capacities[i]);
        if (chs_err) return 1;
    }
    
    for (int i = 0; i < num_requests; i++)
    {
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

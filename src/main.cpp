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
    return key / 2;
}


bool read_config(const std::string& filename, int& level_count,
    std::vector<std::string>& algorithms, std::vector<std::size_t>& capacities,
    int& num_requests, std::vector<int>& requests, std::string& err)
{
    std::ifstream config(filename);
    if (!config.is_open())
    {
        err = "cannot open file: " + filename;
        return false;
    }

    if (!(config >> level_count))
    {
        err = "missing level count";
        return false;
    }

    if (level_count <= 0)
    {
        err = "level count < 1";
        return false;
    }

    static const char* known_algorithms[] = {"LRU", "LFU", "LIRS", "ARC", "2Q"};

    algorithms.resize(level_count);
    for (int i = 0; i < level_count; i++)
    {
        if (!(config >> algorithms[i]))
        {
            err = "missing algorithm for level " + std::to_string(i);
            return false;
        }

        bool known = false;

        for (const char* x : known_algorithms)
        {
            if (algorithms[i] == x)
            {
                known = true;
                break;
            }
        }

        if (!known)
        {
            err = "unknown algorithm at level " + std::to_string(i) + ": '" + algorithms[i] + "'";
            return false;
        }
    }

    capacities.resize(level_count);
    for (int i = 0; i < level_count; i++)
    {
        if (!(config >> capacities[i]))
        {
            err = "missing capacity for level " + std::to_string(i);

            return false;
        }

        const int e = validate_capacity(algorithms[i], capacities[i]);

        if (e != NO_ERR)
        {
            err = "invalid capacity for " + algorithms[i]
                + " at level " + std::to_string(i)
                + ": " + std::to_string(capacities[i]);

            
            return false;
        }
    }

    if (!(config >> num_requests))
    {
        err = "missing request count";

        return false;
    }

    requests.resize(num_requests);
    for (int i = 0; i < num_requests; i++)
    {
        if (!(config >> requests[i]))
        {
            err = "missing request #" + std::to_string(i);
            return false;
        }
    }

    return true;
}



int main(const int argc, const char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "No file specified" << std::endl;

        return 2;
    }


    int level_count = 0;
    int num_requests = 0;

    std::vector<std::string> algorithms;
    std::vector<std::size_t> capacities;
    std::vector<int> requests;

    std::string err;
    if (!read_config(argv[1], level_count, algorithms, capacities, num_requests, requests, err))
    {
        std::cerr << "error: " << err << '\n';

        return 1;
    }

    



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

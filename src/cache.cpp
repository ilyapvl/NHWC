#include "cache.h"

namespace cache
{

std::string algorithm_from_enum(CacheAlgorithm algorithm)
{
    switch (algorithm)
    {
        case CacheAlgorithm::LRU: return "LRU"; break;
        case CacheAlgorithm::LFU: return "LFU"; break;
        case CacheAlgorithm::LIRS: return "LIRS"; break;
        case CacheAlgorithm::ARC: return "ARC"; break;
        case CacheAlgorithm::TwoQ: return "2Q"; break;
        case CacheAlgorithm::Invalid: return "invalid"; break;
    }
}

CacheAlgorithm algorithm_from_string(const std::string& algorithm)
{

    if (algorithm == "LRU") return CacheAlgorithm::LRU;
    else if (algorithm == "LFU") return CacheAlgorithm::LFU;
    else if (algorithm == "LIRS") return CacheAlgorithm::LIRS;
    else if (algorithm == "ARC") return CacheAlgorithm::ARC;
    else if (algorithm == "2Q") return CacheAlgorithm::TwoQ;

    return CacheAlgorithm::Invalid;
}

int validate_capacity(CacheAlgorithm algorithm, long long int capacity)
{
    if (capacity < 0 && algorithm == CacheAlgorithm::LRU) return INVALID_CAPACITY;
    else if (capacity < 2 && algorithm == CacheAlgorithm::TwoQ) return INVALID_CAPACITY;
    else if (capacity < 1 && algorithm != CacheAlgorithm::LRU && algorithm != CacheAlgorithm::TwoQ) return INVALID_CAPACITY;
    

    return NO_ERR;
}


}

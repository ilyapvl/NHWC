#ifndef CACHE_H
#define CACHE_H

#include <iostream>
#include <optional>
#include <list>
#include <unordered_map>
#include <map>
#include <cassert>
#include <memory>
#include <string>
#include <limits>
#include <cstddef>
#include <utility>

#include "ghost_info.h"

namespace cache
{

enum cache_err
{
    NO_ERR = 0,
    INVALID_CAPACITY = 1,
};

enum class CacheAlgorithm { LRU, LFU, LIRS, ARC, TwoQ, Invalid };

std::string algorithm_from_enum(CacheAlgorithm algorithm);

CacheAlgorithm algorithm_from_string(const std::string& algorithm);

int validate_capacity(CacheAlgorithm algorithm, long long int capacity);


template <typename K, typename V>
class Cache
{
protected:
    std::size_t m_capacity;
    std::unordered_map<K, std::unique_ptr<const V>> m_values;

public:
    Cache(std::size_t capacity);
    virtual ~Cache() = default;

    std::size_t capacity() const;

    virtual bool contains(const K& key) const = 0;


    virtual std::unique_ptr<const V> extract_ptr(const K& key) = 0;
    virtual std::optional<std::pair<K, std::unique_ptr<const V>>> insert_ptr(const K& key, std::unique_ptr<const V> vptr) = 0;
    virtual std::optional<V> get(const K& key) = 0;

    std::optional<std::pair<K, V>> insert(const K& key, const V& value)
    {
        auto p = std::make_unique<const V>(value);
        auto r = insert_ptr(key, std::move(p));

        if (!r) return std::nullopt;
        return std::make_pair(r->first, *r->second);
    }
    

    virtual void dump(std::ostream& out) const = 0;



    V access(const K& key, std::function<V(const K&)> slow_get_page)
    {
        if (auto v = get(key)) return *v;

        V value = slow_get_page(key);
        insert(key, value);
        return value;
    }


    const std::unordered_map<K, std::unique_ptr<const V>>& debug_values() const
    {
        return m_values;
    }
};


}

#include "cache.tpp"

#endif // CACHE_H

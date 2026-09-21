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
        auto p = std::make_unique<const V>(std::move(value));
        auto r = insert_ptr(key, std::move(p));

        if (!r) return std::nullopt;
        return std::make_pair(r->first, V(*r->second));
    }

    std::optional<V> extract(const K& key)
    {
        auto p = extract_ptr(key);
        
        if (!p) return std::nullopt;
        return *p;
    }


    virtual void dump(std::ostream& out) const = 0;
};

#include "cache.tpp"

#endif // CACHE_H

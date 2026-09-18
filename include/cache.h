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
    std::unordered_map<K, V> m_values;

public:
    Cache(std::size_t capacity);
    virtual ~Cache() = default;

    std::size_t capacity() const;

    virtual bool contains(const K key) const = 0;

    virtual void extract(const K key) = 0; //TODO - maybe return std::optional
    virtual std::optional<std::pair<K, V>> insert(const K key, const V value, const bool is_user_request) = 0; //TODO - no need for is_user_request
    virtual std::optional<V> get(const K key) const = 0;
    virtual void touch(const K key) {};


    virtual void dump(std::ostream& out) const = 0;
};

#include "cache.tpp"

#endif // CACHE_H

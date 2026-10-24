#ifndef CACHE_SYSTEM_INVARIANTS_H
#define CACHE_SYSTEM_INVARIANTS_H

#include <gtest/gtest.h>
#include <string>
#include <unordered_set>

#include "cache_system.h"
#include "cache_test_invariants.h"

namespace cache_test
{

void check_system_structure(const cache::CacheSystem<int, int>& sys, const std::string& what)
{
    SCOPED_TRACE(what);

    const auto& levels = sys.levels();

    // no duplicates across system
    std::unordered_set<int> all_keys;
    for (std::size_t i = 0; i < levels.size(); i++)
    {
        const auto& vals = levels[i]->debug_values();
        for (const auto& [k, _] : vals)
        {
            EXPECT_TRUE(all_keys.insert(k).second)
                << "key " << k << " appears in level " << i
                << " but was already found in another level";
        }
    }

    // hits > 0
    for (std::size_t i = 0; i < levels.size(); i++)
    {
        EXPECT_GE(sys.get_hits(static_cast<int>(i)), 0) << "level " << i << " has negative hit counter";
    }

    // invariants of each level
    for (std::size_t i = 0; i < levels.size(); i++)
    {
        check_invariants(*levels[i], sys.m_algorithms[i], " internal check for level " + std::to_string(i) + "\n");
    }
}

} // namespace cache_test

#endif

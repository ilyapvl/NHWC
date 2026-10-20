#ifndef CACHE_TEST_INVARIANTS_H
#define CACHE_TEST_INVARIANTS_H

#include <gtest/gtest.h>

#include <algorithm>
#include <list>
#include <string>
#include <unordered_set>
#include <vector>

#include "cache.h"
#include "lru_cache.h"
#include "lfu_cache.h"
#include "lirs_cache.h"
#include "arc_cache.h"
#include "twoq_cache.h"

namespace cache_test {

// ===========================================================================
// LRU
// ===========================================================================

void check_lru_structure(const cache::LRUCache<int,int>& c, const std::string& what)
{
    SCOPED_TRACE(what);

    const auto& order = c.debug_order();
    const auto& pos   = c.debug_positions();
    const auto& vals  = c.debug_values();

    EXPECT_EQ(order.size(), pos.size())   << "order vs positions size";
    EXPECT_EQ(order.size(), vals.size())  << "order vs values size";
    EXPECT_LE(order.size(), c.capacity()) << "size exceeds capacity";

    // no duplicates
    std::unordered_set<int> seen;
    for (int k : order)
    {
        EXPECT_TRUE(seen.insert(k).second) << "duplicate in order: " << k;
    }

    // key without value/position
    for (auto it = order.begin(); it != order.end(); it++)
    {
        auto pit = pos.find(*it);
        ASSERT_NE(pit, pos.end()) << "resident key " << *it << " not in positions";
        EXPECT_EQ(pit->second, it) << "iterator mismatch for " << *it;

        auto vit = vals.find(*it);
        ASSERT_NE(vit, vals.end()) << "resident key " << *it << " has no value";
        EXPECT_TRUE(vit->second) << "null value for " << *it;
    }

    // values[] <-> positions[] <-> order[]
    for (const auto& [k, _] : pos)
    {
        EXPECT_NE(vals.find(k), vals.end()) << "position without value: " << k;
        EXPECT_TRUE(seen.count(k)) << "position without order entry: " << k;
    }
    for (const auto& [k, _] : vals)
    {
        EXPECT_NE(pos.find(k), pos.end()) << "value without position: " << k;
        EXPECT_TRUE(seen.count(k)) << "value without order entry: " << k;
    }
}

// ===========================================================================
// LFU
// ===========================================================================

void check_lfu_structure(const cache::LFUCache<int,int>& c, const std::string& what)
{
    SCOPED_TRACE(what);

    const auto& ftk  = c.debug_freq_to_keys();
    const auto& ktp  = c.debug_key_to_pair();
    const auto& vals = c.debug_values();

    //sizes
    EXPECT_EQ(ktp.size(), vals.size())         << "key_to_pair vs values size";
    EXPECT_LE(ktp.size(), c.capacity())        << "size exceeds capacity";
    EXPECT_LE(c.debug_ghost_size(), c.debug_ghost_capacity()) << "ghost exceeds capacity";

    // all freqs > 0, no duplicate keys in same freq
    std::size_t total_in_buckets = 0;
    for (const auto& [freq, lst] : ftk)
    {
        EXPECT_GT(freq, 0)          << "non-positive freq: " << freq;
        EXPECT_FALSE(lst.empty())   << "empty bucket at freq " << freq;

        std::unordered_set<int> within;
        for (int k : lst)
        {
            EXPECT_TRUE(within.insert(k).second)
                << "duplicate key " << k << " in bucket " << freq;
        }
        total_in_buckets += lst.size();
    }

    // sum of all freqs sizes = total size
    EXPECT_EQ(total_in_buckets, ktp.size()) << "buckets hold a different count than the map";

    // ach key in a bucket is in key_to_pair with matching freq and iterator
    for (const auto& [freq, lst] : ftk)
    {
        for (auto it = lst.begin(); it != lst.end(); it++)
        {
            auto kit = ktp.find(*it);
            ASSERT_NE(kit, ktp.end())
                << "key " << *it << " in bucket " << freq << " but not in key_to_pair";
            EXPECT_EQ(kit->second.first, freq)
                << "freq mismatch for " << *it;
            EXPECT_EQ(kit->second.second, it)
                << "iterator mismatch for " << *it;
        }
    }

    // each entry of key_to_pair is present in the bucket it claims
    for (const auto& [k, pair] : ktp)
    {
        const int f = pair.first;
        auto bit = ftk.find(f);
        ASSERT_NE(bit, ftk.end()) << "bucket " << f << " missing for key " << k;

        const auto& bucket = bit->second;
        EXPECT_NE(std::find(bucket.begin(), bucket.end(), k), bucket.end())
            << "key " << k << " not in its claimed bucket " << f;

        ASSERT_NE(vals.find(k), vals.end()) << "resident key " << k << " has no value";
        EXPECT_TRUE(vals.at(k)) << "null value for " << k;
    }

    // no extra keys in values
    for (const auto& [k, _] : vals)
    {
        EXPECT_NE(ktp.find(k), ktp.end()) << "value without key_to_pair entry: " << k;
    }

    // no residents in system_ghost
    for (const auto& [k, _] : vals)
    {
        EXPECT_FALSE(c.debug_in_ghost(k)) << "resident key " << k << " is also in system ghost";
    }
}

// ===========================================================================
// LIRS
// ===========================================================================

std::size_t lirs_expected_lir_capacity(std::size_t capacity)
{
    if (capacity < 2) return 0;
    return capacity - std::max(cache::LIRSCache<int,int>::MIN_HIR, capacity / cache::LIRSCache<int,int>::LIR_HIR_RATIO);
}

std::size_t lirs_expected_hir_capacity(std::size_t capacity)
{
    return capacity - lirs_expected_lir_capacity(capacity);
}

void check_lirs_structure(const cache::LIRSCache<int,int>& c, const std::string& what)
{
    SCOPED_TRACE(what);

    const auto& stack = c.debug_stack();
    const auto& queue = c.debug_queue();
    const auto& infos = c.debug_element_infos();
    const auto& vals  = c.debug_values();

    const std::size_t cap         = c.capacity();
    const std::size_t exp_lir_cap = lirs_expected_lir_capacity(cap);
    const std::size_t exp_hir_cap = lirs_expected_hir_capacity(cap);

    // lir_capacity matches the derived from LIR_HIR_RATIO / MIN_HIR
    EXPECT_EQ(c.debug_lir_capacity(), exp_lir_cap) << "lir_capacity formula broken for cap=" << cap;

    // counters and sizes
    EXPECT_LE(c.debug_resident_count(), cap)                  << "resident > capacity";
    EXPECT_LE(c.debug_lir_count(), c.debug_lir_capacity())    << "LIR > LIR capacity";
    EXPECT_LE(c.debug_lir_count(), c.debug_resident_count())  << "LIR > resident";
    EXPECT_EQ(c.debug_resident_count(), vals.size())          << "resident count vs values";
    EXPECT_LE(c.debug_ghost_size(), c.debug_ghost_capacity()) << "ghost > capacity";

    // every resident is either LIR or resident HIR, and resident HIRs are in the queue
    EXPECT_EQ(queue.size(), c.debug_resident_count() - c.debug_lir_count())
        << "queue size != resident_count - lir_count";

    // LIR == statck.size
    EXPECT_LE(c.debug_lir_count(), stack.size()) << "stack smaller than LIR set";

    // stack <= 3 * cap
    EXPECT_LE(stack.size(), 3 * cap) << "stack > 3 * capacity";

    if (cap >= 2)
    {
        EXPECT_GE(exp_hir_cap, (cache::LIRSCache<int,int>::MIN_HIR))
            << "target HIR capacity below MIN_HIR for cap=" << cap;
    }

    if (c.debug_resident_count() == cap && cap >= 2)
    {
        EXPECT_LT(c.debug_lir_count(), cap)     << "full cache is all-LIR";
        EXPECT_FALSE(queue.empty())             << "full cache has empty HIR queue";
        EXPECT_GE(queue.size(), exp_hir_cap)    << "full cache: queue below target HIR capacity";
    }

    // no duplicates in stack
    std::unordered_set<int> stack_set;
    stack_set.reserve(stack.size() * 2);
    for (int k : stack)
    {
        EXPECT_TRUE(stack_set.insert(k).second) << "duplicate in stack: " << k;

        auto it = infos.find(k);
        ASSERT_NE(it, infos.end()) << "stack key " << k << " missing from infos";
        EXPECT_TRUE(it->second.in_stack)      << "in_stack false for stack key " << k;
        EXPECT_EQ(*it->second.stack_it, k)    << "stack_it mismatch for " << k;
    }

    // no duplicates in queue
    std::unordered_set<int> queue_set;
    queue_set.reserve(queue.size() * 2);
    for (int k : queue)
    {
        EXPECT_TRUE(queue_set.insert(k).second) << "duplicate in queue: " << k;

        auto it = infos.find(k);
        ASSERT_NE(it, infos.end()) << "queue key " << k << " missing from infos";
        EXPECT_TRUE(it->second.in_queue)      << "in_queue false for queue key " << k;
        EXPECT_TRUE(it->second.resident)      << "queue key " << k << " is not resident";
        EXPECT_FALSE(it->second.lir)          << "LIR key in HIR queue: " << k;
        EXPECT_EQ(*it->second.queue_it, k)    << "queue_it mismatch for " << k;
        EXPECT_NE(vals.find(k), vals.end())   << "queue key " << k << " has no value";
    }

    // infos check
    std::size_t resident_cnt = 0;
    std::size_t lir_cnt      = 0;
    std::size_t in_stack_cnt = 0;
    std::size_t in_queue_cnt = 0;

    for (const auto& [k, info] : infos)
    {
        if (info.resident) resident_cnt++;
        if (info.lir)      lir_cnt++;
        if (info.in_stack) in_stack_cnt++;
        if (info.in_queue) in_queue_cnt++;

        EXPECT_EQ(info.in_stack, stack_set.count(k) > 0)        << "in_stack flag mismatch for " << k;
        EXPECT_EQ(info.in_queue, queue_set.count(k) > 0)        << "in_queue flag mismatch for " << k;
        EXPECT_EQ(info.resident, vals.find(k) != vals.end())    << "resident flag mismatch for " << k;

        if (info.lir)
        {
            EXPECT_TRUE(info.resident) << "LIR " << k << " is not resident";
            EXPECT_TRUE(info.in_stack) << "LIR " << k << " is not in stack";
        }

        if (info.resident)
        {
            auto vit = vals.find(k);
            ASSERT_NE(vit, vals.end()) << "resident " << k << " has no value";
            EXPECT_TRUE(vit->second)   << "null value for " << k;
        }
    }

    EXPECT_EQ(resident_cnt, c.debug_resident_count()) << "resident counter mismatch";
    EXPECT_EQ(lir_cnt,      c.debug_lir_count())      << "LIR counter mismatch";
    EXPECT_EQ(stack.size(), in_stack_cnt)             << "stack size vs in_stack flags";
    EXPECT_EQ(queue.size(), in_queue_cnt)             << "queue size vs in_queue flags";


    std::unordered_set<int> union_set = stack_set;
    union_set.insert(queue_set.begin(), queue_set.end());
    EXPECT_EQ(infos.size(), union_set.size()) << "element_infos size does not match total keys in S + Q";
    

    // values[] <-> infos[]
    for (const auto& [k, _] : vals)
    {
        auto it = infos.find(k);
        ASSERT_NE(it, infos.end()) << "value without element_info: " << k;
        EXPECT_TRUE(it->second.resident) << "value for non-resident key: " << k;
    }

    // no residents is system_ghost
    for (const auto& [k, _] : vals)
    {
        EXPECT_FALSE(c.debug_in_ghost(k)) << "resident key " << k << " is also in system ghost";
    }
}

// ===========================================================================
// ARC
// ===========================================================================

constexpr int ARC_UNKNOWN = -1;
constexpr int ARC_NONE    = 0;
constexpr int ARC_T1      = 1;
constexpr int ARC_T2      = 2;
constexpr int ARC_B1      = 3;
constexpr int ARC_B2      = 4;

const char* arc_tag_name(int tag)
{
    switch (tag)
    {
        case ARC_UNKNOWN: return "UNKNOWN";
        case ARC_NONE:    return "NONE";
        case ARC_T1:      return "T1";
        case ARC_T2:      return "T2";
        case ARC_B1:      return "B1";
        case ARC_B2:      return "B2";
    }
    return "?";
}

void check_arc_structure(const cache::ARCCache<int,int>& c, const std::string& what)
{
    SCOPED_TRACE(what);

    const auto& t1    = c.debug_t1();
    const auto& t2    = c.debug_t2();
    const auto& b1    = c.debug_b1();
    const auto& b2    = c.debug_b2();
    const auto& infos = c.debug_element_infos();
    const auto& vals  = c.debug_values();

    const std::size_t cap = c.capacity();

    // arc list sizes
    EXPECT_LE(t1.size() + t2.size(), cap) << "|T1| + |T2| > capacity";
    EXPECT_LE(t1.size() + b1.size(), cap) << "|T1| + |B1| > capacity";
    EXPECT_LE(t1.size() + t2.size() + b1.size() + b2.size(), 2 * cap) << "|T1| + |T2| + |B1| + |B2| > 2 * capacity";
    EXPECT_LE(c.debug_target_t1_size(), cap) << "target_t1_size > capacity";

    // values mirror resident set
    EXPECT_EQ(vals.size(), t1.size() + t2.size()) << "values size != |T1| + |T2|";

    // t1 > 0 
    EXPECT_GE(c.debug_target_t1_size(), 0.0) << "target_t1_size < 0";

    // each list has no duplicates
    std::unordered_set<int> all_seen;

    auto walk = [&](const std::list<int>& lst, int expected_tag, bool resident)
    {
        std::unordered_set<int> local;
        for (int k : lst)
        {
            EXPECT_TRUE(local.insert(k).second) << "duplicate in list " << arc_tag_name(expected_tag) << ": " << k;
            EXPECT_TRUE(all_seen.insert(k).second) << "key " << k << " appears in more than one list";

            const int tag = c.debug_list_tag(k);
            EXPECT_EQ(tag, expected_tag) << "list tag mismatch for " << k
                << " (expected " << arc_tag_name(expected_tag)
                << ", got " << arc_tag_name(tag) << ")";

            EXPECT_TRUE(c.debug_iterator_valid(k)) << "iterator invalid for " << k;
            EXPECT_EQ(c.contains(k), resident) << "residency mismatch for " << k;

            if (resident)
            {
                auto vit = vals.find(k);
                ASSERT_NE(vit, vals.end()) << "resident key " << k << " has no value";
                EXPECT_TRUE(vit->second)  << "null value for resident " << k;
            }
            else
            {
                EXPECT_EQ(vals.find(k), vals.end()) << "ghost key " << k << " has a value";
            }
        }
    };

    walk(t1, ARC_T1, true);
    walk(t2, ARC_T2, true);
    walk(b1, ARC_B1, false);
    walk(b2, ARC_B2, false);

    std::size_t num_infos = 0;
    for (const auto& [k, info] : infos)
    {
        num_infos++;

        EXPECT_TRUE(all_seen.count(k) > 0) << "key " << k << " in element_infos but not in any list";

        const int  tag             = c.debug_list_tag(k);
        const bool is_resident_tag = (tag == ARC_T1 || tag == ARC_T2);

        EXPECT_EQ(info.resident, is_resident_tag) << "resident flag disagrees with list tag for key " << k;

        if (info.resident)
            EXPECT_NE(vals.find(k), vals.end()) << "resident key " << k << " has no value";
        else
            EXPECT_EQ(vals.find(k), vals.end()) << "ghost key " << k << " has a value";
    }

    EXPECT_EQ(num_infos, all_seen.size()) << "element_infos size does not match total keys in lists";

    // values[] contain elements only from T1/T2
    for (const auto& [k, _] : vals)
    {
        const int tag = c.debug_list_tag(k);
        EXPECT_TRUE(tag == ARC_T1 || tag == ARC_T2)
            << "value for non-resident key " << k
            << " (tag " << arc_tag_name(tag) << ")";
    }

    // no residents in system_ghost
    for (const auto& [k, _] : vals)
    {
        EXPECT_FALSE(c.debug_in_ghost(k)) << "resident key " << k << " is also in system ghost";
    }
}

// ===========================================================================
// 2Q
// ===========================================================================

constexpr int TWOQ_NOT_FOUND = -1;
constexpr int TWOQ_NONE      = 0;
constexpr int TWOQ_Q1        = 1;
constexpr int TWOQ_Q2        = 2;
constexpr int TWOQ_GHOST     = 3;

const char* twoq_tag_name(int tag)
{
    switch (tag)
    {
        case TWOQ_NOT_FOUND: return "NOT_FOUND";
        case TWOQ_Q1:        return "Q1";
        case TWOQ_Q2:        return "Q2";
        case TWOQ_GHOST:     return "GHOST";
    }
    return "?";
}

inline void check_twoq_structure(const cache::TwoQCache<int,int>& c, const std::string& what)
{
    SCOPED_TRACE(what);

    const auto& q1    = c.debug_q1();
    const auto& q2    = c.debug_q2();
    const auto& ghost = c.debug_ghost();
    const auto& vals  = c.debug_values();

    const std::size_t cap = c.capacity();

    // target sizes
    EXPECT_EQ(c.debug_target_q1_size(),    std::max<std::size_t>(1, cap / 4)) << "target_q1_size broken";
    EXPECT_EQ(c.debug_target_ghost_size(), std::max<std::size_t>(1, cap / 2)) << "target_ghost_size broken";

    // values mirror resident set
    EXPECT_LE(q1.size() + q2.size(), cap) << "resident set > capacity";
    EXPECT_LE(ghost.size(), c.debug_target_ghost_size()) << "ghost > target_ghost_size";

    EXPECT_EQ(vals.size(), q1.size() + q2.size()) << "values size != Q1 + Q2";

    //no duplicates
    std::unordered_set<int> all_seen;

    auto walk = [&](const std::list<int>& lst, int expected_tag, bool resident)
    {
        std::unordered_set<int> local;
        for (int k : lst)
        {
            EXPECT_TRUE(local.insert(k).second) << "duplicate in list " << twoq_tag_name(expected_tag) << ": " << k;
            EXPECT_TRUE(all_seen.insert(k).second) << "key " << k << " appears in more than one list";

            EXPECT_EQ(c.debug_list_tag(k), expected_tag) << "tag mismatch for key " << k;
            EXPECT_TRUE(c.debug_iterator_valid(k)) << "iterator invalid for " << k;
            EXPECT_EQ(c.contains(k), resident) << "residency mismatch for " << k;

            const bool has_value = vals.find(k) != vals.end();

            if (resident)
            {
                EXPECT_TRUE(has_value) << "resident key " << k << " has no value";
                if (has_value) EXPECT_TRUE(vals.at(k)) << "null value for resident " << k;
            }
            else
            {
                EXPECT_FALSE(has_value) << "ghost key " << k << " has a value";
            }
        }
    };

    walk(q1,    TWOQ_Q1,    true);
    walk(q2,    TWOQ_Q2,    true);
    walk(ghost, TWOQ_GHOST, false);

    // infos check
    EXPECT_EQ(all_seen.size(), c.debug_infos_size()) << "element_infos has keys missing from all lists";

    for (const auto& [k, _] : vals)
    {
        const int tag = c.debug_list_tag(k);
        EXPECT_TRUE(tag == TWOQ_Q1 || tag == TWOQ_Q2)
            << "value for non-resident key " << k
            << " (tag " << twoq_tag_name(tag) << ")";
    }

    for (const auto& [k, _] : vals)
    {
        EXPECT_FALSE(c.debug_in_ghost(k)) << "resident key " << k << " is also in system ghost";
    }
}


} // namespace cache_test

#endif // CACHE_TEST_INVARIANTS_H

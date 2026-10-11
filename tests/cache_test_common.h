#ifndef CACHE_TEST_COMMON_H
#define CACHE_TEST_COMMON_H

#include <gtest/gtest.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>


struct InvParams
{
    std::size_t   cap;
    int           key_range;
    int           ops;
    std::uint32_t seed;
};


inline void PrintTo(const InvParams& p, std::ostream* os)
{
    *os << "cap=" << p.cap
        << " key_range=" << p.key_range
        << " ops=" << p.ops
        << " seed=" << p.seed;
}



template <typename CacheT>
class CacheInvTest : public ::testing::TestWithParam<InvParams>
{
protected:
    std::unique_ptr<CacheT> cache;

    void SetUp() override
    {
        cache = std::make_unique<CacheT>(GetParam().cap);
    }

    void TearDown() override
    {
        if (::testing::Test::HasFailure() && cache)
            cache->dump(std::cerr);
    }
};


inline auto StandardInvParams()
{
    return ::testing::Values(
        // cap key_range ops seed
        InvParams{3,        50,     5000,       1},
        InvParams{4,        8,      5000,       2},
        InvParams{7,        1000,   20000,      3},
        InvParams{64,       256,    50000,      4},
        InvParams{1024,     4096,   100000,     5},
        InvParams{1024,     8192,   100000,     6});
}

#endif

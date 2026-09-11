#include <gtest/gtest.h>
#include "lru_cache.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <optional>

struct Operation
{
    enum class Func { Insert, Get, Extract, Contains, Invalid };

    Func func;
    int key = 0;
    int value = 0;

    std::string expected;
    
    
    int erase_key   = 0;
    int erase_value = 0;
};

Operation::Func func_from_string(const std::string& s)
{
    if (s == "insert") return Operation::Func::Insert;
    else if (s == "get") return Operation::Func::Get;
    else if (s == "extract") return Operation::Func::Extract;
    else if (s == "contains") return Operation::Func::Contains;

    return Operation::Func::Invalid;
}

std::vector<Operation> parse_file(const std::string path, std::size_t& capacity)
{
    std::ifstream in(path);

    std::vector<Operation> ops;
    std::string line;

    while (std::getline(in, line))
    {
        std::istringstream iss(line);
        std::string first;
        if (!(iss >> first)) continue;

        if (first == "capacity")
        {
            iss >> capacity;
            continue;
        }
        

        Operation op;
        op.func = func_from_string(first);

        if (op.func == Operation::Func::Insert)
        {
            iss >> op.key >> op.value;

        }
        else
        {
            iss >> op.key;
        }

        std::string arrow;
        iss >> arrow;

        iss >> op.expected;

        if (op.expected == "erase")
        {
            iss >> op.erase_key >> op.erase_value;
        }

        ops.push_back(op);
    }

    return ops;
}


TEST(LRUFromFlle, SequenceTest)
{
    const std::string path = std::string(TEST_DATA_DIR) + "/lru_sequence.txt";

    std::size_t capacity = 0;
    std::vector<Operation> ops;

    ASSERT_NO_THROW(ops = parse_file(path, capacity));
    ASSERT_GT(capacity, 0u);
    ASSERT_FALSE(ops.empty());

    LRUCache<int, int> cache(capacity);

    for (std::size_t i = 0; i < ops.size(); ++i)
    {
        const Operation& op = ops[i];

        SCOPED_TRACE("step " + std::to_string(i)+ " op " + std::to_string(static_cast<int>(op.func)) + " key " + std::to_string(op.key));

        switch (op.func)
        {

        case Operation::Func::Insert:
            {
                auto erased = cache.insert(op.key, op.value, true);

                if (op.expected == "none")
                {
                    EXPECT_FALSE(erased.has_value());
                }
                
                else if (op.expected == "erase")
                {
                    ASSERT_TRUE(erased.has_value());
                    EXPECT_EQ(erased->first,  op.erase_key);
                    EXPECT_EQ(erased->second, op.erase_value);
                }
                
                else
                {
                    FAIL() << op.expected;
                }

                EXPECT_TRUE(cache.contains(op.key));
                EXPECT_EQ(cache.get(op.key).value_or(-1), op.value);
                break;
            }



        case Operation::Func::Get:
            {
                auto v = cache.get(op.key);
                if (op.expected == "miss")
                {
                    EXPECT_FALSE(v.has_value());
                }
                
                else
                {
                    ASSERT_TRUE(v.has_value());

                    EXPECT_EQ(v.value(), std::stoi(op.expected));
                }

                break;
            }

        case Operation::Func::Extract:
            {
                cache.extract(op.key);
                break;
            }

        case Operation::Func::Contains:
            {
                const bool expected = (op.expected == "true");

                EXPECT_EQ(cache.contains(op.key), expected);
                break;
            }

        case Operation::Func::Invalid:
            {
                FAIL();
            }

        }
    }
}

#include <gtest/gtest.h>
#include "cache.h"
#include "cache_system.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <optional>

static std::vector<std::string> test_file_paths;

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

struct TestCase
{
    std::string algorithm;
    std::size_t capacity = 0;
    std::vector<Operation> ops;
};


Operation::Func func_from_string(const std::string s)
{
    if (s == "insert") return Operation::Func::Insert;
    else if (s == "get") return Operation::Func::Get;
    else if (s == "extract") return Operation::Func::Extract;
    else if (s == "contains") return Operation::Func::Contains;

    return Operation::Func::Invalid;
}

TestCase parse_file(const std::string path, TestCase& test)
{
    std::ifstream in(path);

    

    if (!in.is_open()) assert(false && "invalid test file");

    std::string line;

    while (std::getline(in, line))
    {
        std::istringstream iss(line);
        std::string first;
        if (!(iss >> first)) continue;

        if (first == "algorithm")
        {
            iss >> test.algorithm;
            continue;
        }

        if (first == "capacity")
        {
            iss >> test.capacity;
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

        test.ops.push_back(op);
    }

    return test;
}

const std::vector<std::string> default_files = {
    "../tests/lru_sequence.txt",
    "../tests/lfu_sequence.txt",
    "../tests/lirs_sequence.txt",
    "../tests/arc_sequence.txt",
    "../tests/twoq_sequence.txt",
};



void run_one_file(const std::string path)
{
    TestCase test;

    ASSERT_NO_THROW(parse_file(path, test));
    ASSERT_GT(test.capacity, 0u);
    ASSERT_FALSE(test.ops.empty());

    std::unique_ptr<Cache<int, int>> cache = make_cache<int, int>(test.algorithm, test.capacity);
    ASSERT_NE(cache, nullptr);

    for (std::size_t i = 0; i < test.ops.size(); ++i)
    {
        const Operation& op = test.ops[i];

        SCOPED_TRACE("step " + std::to_string(i)+ " op " + std::to_string(static_cast<int>(op.func)) + " key " + std::to_string(op.key));

        switch (op.func)
        {

        case Operation::Func::Insert:
            {
                auto erased = cache->insert(op.key, op.value, true);

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

                EXPECT_TRUE(cache->contains(op.key));
                EXPECT_EQ(cache->get(op.key).value_or(-1), op.value);
                break;
            }



        case Operation::Func::Get:
            {
                auto v = cache->get(op.key);
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
                cache->extract(op.key);
                break;
            }

        case Operation::Func::Contains:
            {
                const bool expected = (op.expected == "true");

                EXPECT_EQ(cache->contains(op.key), expected);
                break;
            }

        case Operation::Func::Invalid:
            {
                FAIL();
            }

        }
    }
}


TEST(FromFile, SequenceTest)
{
    
    
    std::vector<std::string> final_files = default_files;

    if (!test_file_paths.empty())
    {
        final_files = test_file_paths;
    }


    for (const auto& f : final_files)
    {
        SCOPED_TRACE("file: " + f);
        run_one_file(f);
    }

}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        for (int i = 1; i < argc; i++) 
        {
            std::string arg = argv[i];
            std::string a;
            
            if (arg.rfind("file=", 0) == 0) test_file_paths.push_back(arg.substr(5));
        }
    }

    

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

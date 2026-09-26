#include <gtest/gtest.h>
#include "cache_system.h"
#include "cache.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstddef>
#include <functional>
#include <iostream>


static std::vector<std::string> test_file_paths;

struct LevelInfo
{
    std::string algorithm;
    std::size_t capacity = 0;
};

struct AccessInfo
{
    int key = 0;
    bool expected_hit = false;
    std::size_t expected_level = 0;
    int expected_value = 0;
};

struct SystemTest
{
    std::size_t levels_count = 0;
    std::vector<LevelInfo> levels;
    std::vector<AccessInfo> requests;
};

SystemTest parse_file(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open()) assert(false && "cant open file");

    SystemTest test;
    std::string line;
    std::size_t line_no = 0;

    while (std::getline(in, line))
    {
        ++line_no;
        if (auto p = line.find('#'); p != std::string::npos)
            line.resize(p);

        std::istringstream iss(line);
        std::string first;
        if (!(iss >> first)) continue;

        if (first == "levels")
        {
            iss >> test.levels_count;
        }

        else if (first == "level")
        {
            LevelInfo level;
            iss >> level.algorithm >> level.capacity;
            test.levels.push_back(level);
        }

        else if (first == "access")
        {
            AccessInfo access;
            iss >> access.key;

            std::string arr;
            iss >> arr;

            std::string type;
            iss >> type;

            if (type == "miss")
            {
                access.expected_hit = false;
                iss >> access.expected_value;
            }

            else if (type == "hit")
            {
                access.expected_hit = true;
                iss >> access.expected_level >> access.expected_value;
            }

            test.requests.push_back(access);

            continue;
        }
    }

    return test;
}

void run_one_file(const std::string& path)
{
    SystemTest test;
    test = parse_file(path);


    CacheSystem<int, int> chs(1, [](const int key) { return key; });

    for (const auto& lvl : test.levels)
        chs.add_cache(lvl.algorithm, lvl.capacity);

    for (std::size_t i = 0; i < test.requests.size(); ++i)
    {
        const AccessInfo& access = test.requests[i];

        SCOPED_TRACE("file=" + path +
                     " step=" + std::to_string(i) +
                     " key=" + std::to_string(access.key));

        const int value = chs.access(access.key);

        EXPECT_EQ(value, access.expected_value);

        if (access.expected_hit)
        {
            EXPECT_EQ(chs.get_last_hit_level(), static_cast<int>(access.expected_level));
        }
    }
}

TEST(CacheSystemFromFile, SequenceTest)
{
    const std::vector<std::string> default_files = {
        std::string(TEST_DATA_DIR) + "/system_sequence.txt",
    };

    const std::vector<std::string>& files = test_file_paths.empty() ? default_files : test_file_paths;

    for (const auto& f : files)
    {
        SCOPED_TRACE("file: " + f);
        run_one_file(f);
    }
}

int main(int argc, char** argv)
{
    std::vector<char*> filtered;
    filtered.reserve(static_cast<std::size_t>(argc));
    filtered.push_back(argv[0]);

    const std::string prefix = "--file=";

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg.rfind(prefix, 0) == 0)
        {
            test_file_paths.push_back(arg.substr(prefix.size()));
        }
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

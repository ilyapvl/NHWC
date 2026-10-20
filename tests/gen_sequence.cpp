#include <libCacheSim/cache.h>
#include <libCacheSim/request.h>
#include <libCacheSim/cacheObj.h>
#include <libCacheSim/evictionAlgo.h>
#include "dataStructure/hashtable/hashtable.h"

#include <cstdio>
#include <fstream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

enum class Op { Insert, Get, Contains };
struct Step { Op op; int key; int value; };

std::vector<Step> generate_uniform(std::mt19937& rng, int key_range, int n_ops, double p_ins, double p_get, double p_cnt)
{
    std::uniform_int_distribution<int> kd(0, key_range - 1);
    std::uniform_real_distribution<double> od(0.0, 1.0);
    std::vector<Step> steps;

    steps.reserve(n_ops);
    for (int i = 0; i < n_ops; ++i)
    {
        Step s;
        double r = od(rng);
        if (r < p_ins) { s.op = Op::Insert; s.key = kd(rng); s.value = i * 10; }
        else if (r < p_ins + p_get) { s.op = Op::Get; s.key = kd(rng); }
        else { s.op = Op::Contains; s.key = kd(rng); }
        steps.push_back(s);
    }

    return steps;
}

static int lookup_value(const std::unordered_map<int,int>& values, int key)
{
    auto it = values.find(key);
    return it != values.end() ? it->second : 0;
}

static request_t make_request(int key)
{
    request_t req = {};
    req.obj_id = key;
    req.obj_size = 1;
    return req;
}

static bool hash_present(cache_t* cache, int key)
{
    return chained_hashtable_find_obj_id_v2(cache->hashtable, key) != nullptr;
}

static bool find_present(cache_t* cache, int key)
{
    request_t req = make_request(key);
    return cache->find(cache, &req, false) != nullptr;
}

static void write_prefix(std::ostream& out, const Step& s)
{
    switch (s.op)
    {
    case Op::Insert: out << "ins " << s.key << " " << s.value; break;
    case Op::Get: out << "get " << s.key; break;
    case Op::Contains: out << "cntn " << s.key; break;
    }
}

int main(int argc, char** argv)
{
    if (argc < 10)
    {
        return 1;
    }

    std::string algo = argv[1];
    std::size_t cap = std::stoul(argv[2]);
    int key_range = std::stoi(argv[3]);
    int n_ops = std::stoi(argv[4]);
    int seed = std::stoi(argv[5]);
    double p_ins = std::stod(argv[6]);
    double p_get = std::stod(argv[7]);
    double p_cnt = std::stod(argv[8]);
    std::string out_path = argv[9];

    std::mt19937 rng(seed);
    auto steps = generate_uniform(rng, key_range, n_ops, p_ins, p_get, p_cnt);

    common_cache_params_t params = {};
    params.cache_size = cap;
    params.hashpower = 20;
    params.consider_obj_metadata = false;

    cache_t* cache = nullptr;
    bool use_hash_directly = false;

    if (algo == "LRU") cache = LRU_init(params, nullptr);
    else if (algo == "LFU") { cache = LFU_init(params, nullptr); use_hash_directly = true; }
    else if (algo == "LIRS") cache = LIRS_init(params, nullptr);
    else if (algo == "ARC") cache = ARC_init(params, nullptr);
    else if (algo == "2Q") cache = TwoQ_init(params, nullptr);
    else { std::fprintf(stderr, "unknown algo %s\n", algo.c_str()); return 1; }

    if (!cache) { std::fprintf(stderr, "init failed\n"); return 1; }

    auto present = [&](int key) {
        return use_hash_directly ? hash_present(cache, key) : find_present(cache, key);
    };

    std::ofstream out(out_path);
    out << "algorithm " << algo << "\n";
    out << "capacity " << cap << "\n\n";

    std::unordered_map<int,int> values;

    for (const Step& s : steps)
    {
        write_prefix(out, s);
        out << " >/ ";

        request_t req = make_request(s.key);

        switch (s.op)
        {
        case Op::Insert:
        {
            const bool was_present = present(s.key);

            if (was_present)
            {
                cache->get(cache, &req);
                values[s.key] = s.value;
                out << "none";
            }

            else
            {
                const bool will_evict = (values.size() >= cap);

                std::vector<int> snapshot;

                if (will_evict)
                {
                    snapshot.reserve(values.size());
                    for (const auto& kv : values) snapshot.push_back(kv.first);
                }

                cache->get(cache, &req);

                int victim_key = -1;

                if (will_evict)
                {
                    for (int k : snapshot)
                    {
                        if (!present(k))
                        {
                            victim_key = k;
                            break;
                        }
                    }
                }

                if (victim_key >= 0)
                {
                    out << "erase " << victim_key << " " << lookup_value(values, victim_key);
                    values.erase(victim_key);
                }
                else
                {
                    out << "none";
                }

                values[s.key] = s.value;
            }

            break;
        }

        case Op::Get:
        {
            if (present(s.key))
            {
                cache->get(cache, &req);
                out << values[s.key];
            }
            
            else
            {
                out << "miss";
            }

            break;
        }

        case Op::Contains:
        {
            out << (present(s.key) ? "true" : "false");
            break;
        }
        
        }

        out << "\n";
    }

    cache->cache_free(cache);
    return 0;
}

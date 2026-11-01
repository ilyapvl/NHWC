# NHWC


An implementation of several cache replacement algorithms with a
multi-level system that combines them


## Overview


The project provides five cache replacement policies: LRU, LFU, ARC, 2Q and LIRS.


In addition, an `OptimalCache` implements Belady`s algorithm — it knows
the entire future sequence in advance and gives the theoretical upper bound on
hit rate. It is not meant to be used inside a live cache system,only as a reference.


A `CacheSystem` holds a vector of levels (each one any of the algorithms above) and routes requests through
them. On a hit in a lower level, the entry is promoted upward via a cascading
insert; on eviction, the victim moves down. All levels share the same key/
value types.


Every algorithm inherits from `Cache<K, V>` and
  implements `contains`, `get`, `insert_ptr`, `extract_ptr`, and `dump`. The
  public `insert` wrapper accept plain values; the `_ptr` variants
  work with `unique_ptr<const V>` for faster promotion between levels.


## Build


### Requirements

- CMake 3.20 or newer
- A C++20 compiler

GoogleTest is fetched automatically via CMake`s
`FetchContent`


### Configure and build


```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```


### Running tests


```bash
ctest --test-dir build --output-on-failure
```


This runs unit tests, and file-based scenario tests (see below)


### Running the driver


The `main` binary reads a configuration file describing the cache hierarchy
and a request sequence:


```bash
./build/main path/to/test.txt
```


File format:


```
<num_levels>
<algo_0> <alg_1> ... <algo_{num_levels-1}>
<capacity_0> <capacity_1> ... <capacity_{num_levels-1}>
<num_requests>
<key_0> <key_1> ... <key_{num_requests-1}>
```


Example:


```
2
LRU LFU
500 5000
1000000
1 2 3 4 1 2 3 4 ...
```


The driver prints per-level hit counts, total hits, and the Belady optimum
for the same sequence.


## Project structure


```
.
├── CMakeLists.txt
├── README.md
├── include/
│   ├── cache.h
│   ├── cache.tpp
│   ├── cache_system.h
│   ├── cache_system.tpp
│   ├── lru_cache.h / .tpp
│   ├── lfu_cache.h / .tpp
│   ├── lirs_cache.h / .tpp
│   ├── arc_cache.h / .tpp
│   ├── twoq_cache.h / .tpp
│   ├── opt_cache.h / .tpp
│   └── ghost_info.h
├── src/
│   └── main.cpp
└── tests/
    ├── test_optimal_cache.cpp
    ├── test_system.cpp
    ├── test_cache.cpp
    ├── lru_sequence.txt
    ├── lfu_sequence.txt
    ├── lirs_sequence.txt
    ├── arc_sequence.txt
    ├── twoq_sequence.txt
    └── system_sequence.txt
```


## Algorithms in detail

### LRU


Single list ordered by recency.

**Note:** LRU allows creating a cache with `capacity == 0` that will bypass all insertions to the next level.
That was done for easier testing of higher levels


### LFU


A `std::map<int, std::list<K>>` from frequency to keys with that frequency.


### LIRS


Two lists — a history stack `S` and a queue `Q` of resident HIR
blocks


### ARC


Four lists — `T1` (recent residents), `T2` (frequent residents), `B1` and
`B2` (their ghost counterparts). A target parameter `p` shifts between
recency and frequency.


### 2Q


Two resident lists (for seen-once keys and for seen-twice-or-more)
plus a bounded ghost list

**Note:** Capacity of 2Q cache must be no less than 2 for working properly


### Optimal (Belady)


Keeps the entire request sequence in memory and, on each
miss, evicts the resident whose next use is farthest in the future.
`simulate()` walks the sequence once and returns the number of hits.

**Note:** OptimalCache can`t be used in cache system

**Note:** `simulate()` mutates the internal state and therefore must be called only once




## Cache<K, V> — the common interface


Every algorithm implements this interface. Methods split into two groups:
the public value-based API and the internal pointer-based API used by
CacheSystem.

**Note:** the `***Cache` class constructors are `***Cache(std::size_t capacity, std::size_t ghost_capacity = 0);`. `ghost_capacity` is a capacity of structure used in CacseSystem (see **State preservation**)


### Value-based API


#### `std::optional<std::pair<K, V>> insert(const K& key, const V& value)`

Value based wrapper around `insert-ptr`


#### `std::optional<V> get(const K& key)`

Reads the value and updates the level`s internal state


#### `bool contains(const K& key) const`

Checks whether the key is resident. Does not affect cache state


#### `void clear()`

Resets the cache and its structures

#### `void dump(std::ostream& out) const`

Prints a snapshot of the cache`s internal structures.


### Pointer-based API


These methods exist so that CacheSystem can move values between levels
without copying V


#### `std::optional<std::pair<K, std::unique_ptr<const V>>> insert_ptr(const K& key, std::unique_ptr<const V> value)`

The value is passed and returned through
unique_ptr<const V>, so no copy of V is made


#### `std::unique_ptr<const V> extract_ptr(const K& key)`

Extracts an element from cache.

**Note:** `extract_ptr` is introduced only to use it inside `CacheSystem`. It is not recommended to use it manually since it was not tested


### Standalone usage

A cache can be created using `make_cache(CacheAlgorithm algo, std::size_t capacity)`, file `cache_factory.h`.

#### `V access(const K& key, std::function<V(const K&)> slow_get_page)`

A user request on key `key`. In case of miss, the `slow_get_page` is called. Example:

```
auto stndl = make_cache<int, int>(CacheAlgorithm::LIRS, 10);

stndl->access(1, slow_get_page);

stndl->dump(std::cout);

```


## OptimalCache<K, V>


`OptimalCache(std::size_t capacity, const std::vector<K>& sequence)`

Stores the entire request sequence for offline analysis. Builds an index
from key to the list of positions where it appears. Requires
capacity >= 1.

### Methods

#### `std::size_t simulate()`

Runs over the stored sequence and returns the number of hits.


#### `void dump(std::ostream& out) const`

Prints a snapshot of the cache`s internal structures.

**Note:** Since `OptimalCache` runs all sequence as a whole, the `dump` function makes sense only inside the `simulate` loop





## CacheSystem


`CacheSystem<K, V>` is a container of cache levels. Each level is an
independent `Cache<K, V>` implementation (LRU, LFU, LIRS, ARC, or 2Q).
Levels are indexed from 0 (the topmost, usually smallest and fastest) to
`N−1` (the bottom-most, usually largest and slowest). All levels share the
same key and value types; internally every level stores values as
`unique_ptr<const V>`.


### Construction

#### `CacheSystem(std::function<V(const K&)> slow_get_page)`

Creates an empty system. The argument is a function that will be called in case of miss


The system is built incrementally via `add_cache`:


```cpp
CacheSystem<int, std::string> cs(slow_get_page);

cs.add_cache(CacheAlgorithm::LRU, 500);
cs.add_cache(CacheAlgorithm::LFU, 5000);
cs.add_cache(CacheAlgorithm::ARC, 20000);
```

### Methods


#### `V access(const K& key)`

Returns the value for key, either from one of the levels (a hit) or from
the miss handler (a miss)



### Access flow


`access(key)` is the primary entry point. It returns a `V` by value and
internally does the following:


### Cascade insert


The cascade is the mechanism that keeps levels consistent when entries move
between them



1. The element starts at level 0 with its value.
2. `insert_ptr` puts the element in level 0. If level 0 is not full yet,
   it returns `nullopt` and the cascade stops - the element is safely
   resident and no further level needs to see it.
3. If level 0 was full, its own replacement policy evicts a victim. The
   victim is returned as `(K, unique_ptr<const V>)` and becomes the new
   `moving` element for level 1.
4. The process repeats for every subsequent level. The cascade ends either
   when a level accepts the element without eviction, or when the element
   falls off the bottom level and is discarded.


### State preservation

All caches except LRU somehow rely on frequency. The element state is stored during a single level only. If an element was simply erased from a level,
that information would be lost. To prevent this, each level has its own `SystemGhost` with size of `capacity` that stores states
and restores them if an element is returned quickly enough.






## Tests

Tests are built with GoogleTest and run via CTest:

    ctest --output-on-failure

The suite is split into three groups.

### Unit tests for `OptimalCache` (`test_optimal_cache.cpp`)

Self-contained `TEST` cases with hardcoded sequences. No external files needed.

### Scenario tests for individual caches (`test_cache.cpp`)

Each test is a plain-text file describing a single algorithm run

Header lines:

    algorithm <LRU|LFU|LIRS|ARC|2Q>
    capacity  <N>

Operation lines have the form `<op> <key> [<value>] >/ <expected>`:

- `ins <key> <value> >/ none` — insertion returns no victim.
- `ins <key> <value> >/ erase <evicted_key> <evicted_value>` — insertion
  evicts a specific entry.
- `get <key> >/ <value>` or `get <key> >/ miss`.
- `cntn <key> >/ true` or `cntn <key> >/ false`.

There are 10 included files: `large_***.txt` and `small_***.txt` for each cache. The traces were generated using [libCacheSim](https://github.com/1a1a11a/libCacheSim) (`LRU`, `LFU`, `ARC`, `2Q`, file gen_sequence.cpp`) and [LIRS2](https://github.com/zhongch4g/LIRS2) (LIRS, using `./replace` modified for step-by-step output)

Extra scenarios can be supplied on the command line:

    ./build/test_cache file=path/to/my_sequence.txt

The `file=` prefix is repeatable. When no `file=` argument is given, the built-in list above is used.

### Scenario tests for `CacheSystem` (`test_system.cpp`)

Same idea, but the file describes a whole multi-level system

    levels <N>
    level <algo> <capacity>
    ...
    access <key> -> hit  <level> <value>
    access <key> -> miss <value>

The runner builds the system, replays each `access`, and checks both the
returned value and `get_last_hit_level()`. The default input is
`system_sequence.txt`; additional files are passed the same way:

    ./build/test_system --file=path/to/another_system.txt

Systems with not-LRU levels are too difficult to trace manually, and I could not find any reference implementations. Therefore, `test_system.cpp` only checks the invariants of the system: some key properties + `check_invariants` on each level.

### Debugging a failure

`test_cache.cpp` wraps each step in `SCOPED_TRACE` with the file, step
index, operation and key, so the failing line is visible in the output.
On the first failing assertion the cache state is dumped to `stderr`










## Cache configuration search (cache_bench)


cache_bench is a driver that, given a workload and a multi-level
topology, searches for a good per-level algorithm assignment using hill
climbing and compares the achieved hit rate against Belady's offline
optimum for the same total capacity.


### Two workload modes


#### Synthetic

A file with one workload spec per line:


 <type> <name> key=value ... 


Supported types:


- uniform — keys drawn uniformly from [0, key_max).
- zipf — single Zipf distribution with exponent alpha.
- zipfmulti — alternating Zipf phases; parameters alphas=0.7,1.1,1.5
 and ops_per_phase=N.
- hotcold — mixture: hot_ratio of accesses go to one of hot_keys
 hot keys, the rest to one of cold_keys cold keys.
- scan — hot keys plus a sequential scan of scan_length keys.
- phase — num_phases phases; each phase works over keys_per_phase
 new keys.


Example: file `data/workloads.txt`.


```
./build/cache_bench --workloads data/workloads.txt -L0 128 -T0 1 -L1 2048 -T1 4 -L2 32768 -T2 13 --miss-ns 80
```


#### Real trace

A binary oracleGeneral trace

Example:

```
./build/cache_bench --trace meta_reag.oracleGeneral --trace-max-records 10000000 -L0 128 -T0 100 -L1 2048 -T1 100000 -L2 32768 -T2 1000000 --miss-ns 10000000 --overhead-ns 1000
```


Record format:
{  uint32_t timestamp;  uint64_t obj_id;  uint32_t obj_size;  int64_t next_access_vtime; }; 


In trace mode --ops and --seed are ignored; the sequence length can be either full file or limited with
`--trace-max-records` flag.





### Objective


Each candidate configuration is evaluated by run() from runner.cpp.
It feeds the sequence through a CacheSystem and accumulates simulated
cost in nanoseconds:


- a hit at level i costs the prefix sum of latencies T0..Ti;
- a miss costs the sum of all level latencies plus miss_ns;
- every access adds overhead_ns.


The objective minimised by the search is sim_ns_per_op.


### Search


hill_climb explores the neighbourhood of the current best configuration:


- replace one level's algorithm with any other from the pool;
- append a new level (up to max_levels);
- drop the last level.


Neighbours are deduplicated by their string form. The search stops after
--max-iter iterations or after no_improvement_stop (currently fixed
to 1) non-improving iterations. The benchmark restarts the hill climb
from every algorithm in the pool and keeps the best result per workload.


### CLI


 cache_bench --workloads PATH -L0 <cap> [-L1 <cap> ...] -T0 <ns> [-T1 <ns> ...] --miss-ns <ns> [options] 
 
 cache_bench --trace PATH [--trace-max-records N] -L0 <cap> [-L1 <cap> ...] -T0 <ns> [-T1 <ns> ...] --miss-ns <ns> [options] 


Required:


- --workloads PATH or --trace PATH (mutually exclusive).
- -L{i} N — capacity of level i. 8 levels max
- -T{i} N — latency of level i in nanoseconds. 8 levels max
- --miss-ns N — cost of a slow_get_page() access.


Optional:


- --trace-max-records N — read at most N records from the trace
 (default 0 = whole file)
- --overhead-ns N — per-op overhead (default 20)
- --min-levels N — minimum number of levels (default 1)
- --max-iter N — hill-climb iteration cap per restart
- --ops N — synthetic sequence length (default 1 000 000); ignored
 in trace mode
- --seed S — RNG seed (default 1234); ignored in trace mode
- --algorithm NAME — restrict the algorithm pool (repeatable)
- --csv PATH — append the search log to a CSV file


### Output


For every workload the tool prints the best configuration found, the
simulated cost, the achieved hit rate, the Belady optimum for the same
total capacity, and the relative gap. A summary table is printed at the
end:


 workload algorithms levels hit_rate opt_rate sim_ns/op
 w1 LIRS/LIRS/LIRS 3 0.3417 0.3734 68.3 ph_4x LRU/LRU/LRU 3 0.2401 0.5483 78.6
 w2 LFU/LIRS/LIRS 3 0.5434 0.6848 52.8









 ## Research

### Synthetic

In that part some `workloads.cpp`-generated workloads were studied. The cache is CPU-like 3-tier with parameters:

```
-L0 128   -T0 1
-L1 2048  -T1 4
-L2 32768 -T2 13
--miss-ns 80
```

The workloads are in `data/workloads.txt`.

Results:

| workload | algorithms | levels | hit_rate | opt_rate | sim_ns/op |
|:---------|:-----------|-------:|---------:|---------:|----------:|
| z_040 | LIRS/LIRS/2Q | 3 | 0.2073 | 0.4873 | 81.0 |
| z_050 | LIRS/LIRS/2Q | 3 | 0.2668 | 0.5234 | 75.8 |
| z_060 | LIRS/LIRS/2Q | 3 | 0.3459 | 0.5731 | 68.8 |
| z_070 | ARC/LIRS/2Q | 3 | 0.4448 | 0.6364 | 59.7 |
| z_080 | LFU/LIRS/LIRS | 3 | 0.5631 | 0.7110 | 48.5 |
| z_090 | LFU/LIRS/LIRS | 3 | 0.6889 | 0.7907 | 36.1 |
| z_100 | LFU/LIRS/LIRS | 3 | 0.8032 | 0.8635 | 24.3 |
| hc_15_8k | LIRS/LIRS/2Q | 3 | 0.2144 | 0.4784 | 80.5 |
| hc_25_8k | 2Q/LIRS/LIRS | 3 | 0.3127 | 0.5361 | 72.3 |
| hc_35_8k | LFU/LIRS/LIRS | 3 | 0.4063 | 0.5951 | 64.4 |
| hc_50_8k | LFU/LIRS/LIRS | 3 | 0.5434 | 0.6848 | 52.8 |
| scan_15_8k | LFU/LIRS/LIRS | 3 | 0.1420 | 0.1458 | 86.2 |
| scan_25_8k | LFU/LIRS/LIRS | 3 | 0.2445 | 0.2463 | 77.6 |
| scan_35_8k | LFU/LIRS/LIRS | 3 | 0.3453 | 0.3463 | 69.2 |
| ph_3x | LRU/LRU/LRU | 3 | 0.3197 | 0.6251 | 72.1 |
| ph_4x | LRU/LRU/LRU | 3 | 0.2401 | 0.5483 | 78.6 |
| ph_6x | LRU/LRU/LRU | 3 | 0.1597 | 0.4496 | 85.1 |


#### zipf
L0: transitions LIRS to ARC to LFU. Weak signal (alpha ≤ 0.6) is LIRS: binary LIR/HIR is suitable when frequency can't yet rank keys. Mid signal (alpha = 0.7) — ARC's adaptive p works. Strong signal (alpha ≥ 0.8) — LFU because frequency matters more.

L1: LIRS in all 9 workloads

L2: 2Q for weak-mid Zipf (α ≤ 0.7), LIRS for strong Zipf (α ≥ 0.8). Pattern: L2 follows L0. When L0 = LIRS (weak), 2Q handles the tail with Q1/Q2 — the trace contains lots of "seen once" keys and 2Q filters them. When L0 = LFU (strong), LIRS at L2 adds protection to the remaining keys that don't fit in L1


#### hotcold
Looks very similar to zipf, only 2Q in L0 differs.


#### scan
L0: LFU only - works as intended filtering scan-keys with `freq = 1`.
L1,L2: LIRS - looks like S stack of LIRS can keep the information longer and therefore distinguish hot keys which don't fit in L0
Very small difference between real and optimal caches.


#### phase shift
LRU on all levels. Phase size is larger than the cache capacity; any information that protects already-seen keys affects negatively






### Real trace
I decided to study `meta_reag.oracleGeneral` trace of CDN requests. You can download it by running

```
curl -O https://s3.amazonaws.com/cache-datasets/cache_dataset_oracleGeneral/2022_metaCDN/meta_reag.oracleGeneral.zst
zstd meta_reag.oracleGeneral.zst
```
`object_size` parameter is discarded and all requests are considered equal

I've used first 25'000'000 requests. There are 7'450'701 unique keys in those, meaning very few repeatedly requested keys

#### 3 levels
Cache is 3-tier memory-like with parameters

```
-L0 <L0>   -T0 100
-L1 <L1>   -T1 100'000
-L2 <L2>   -T2 1'000'000
--miss-ns 10'000'000
--overhead-ns 1000
```

and different capacities.

Results:

| workload | algorithms | levels | hit_rate | opt_rate | sim_ns/op | L0 | L1 | L2 |
|:---------|:-----------|-------:|---------:|---------:|----------:|---:|---:|---:|
| meta_reag.oracleGeneral | 2Q/ARC/2Q | 3 | 0.5262 | 0.5717 | 5307016.1 | 64 | 256 | 1024 |
| meta_reag.oracleGeneral | 2Q/2Q/2Q | 3 | 0.5396 | 0.5865 | 5151101.2 | 128 | 512 | 2048 |
| meta_reag.oracleGeneral | LRU/LIRS/2Q | 3 | 0.5519 | 0.6015 | 5011653.8 | 256 | 1024 | 4096 |
| meta_reag.oracleGeneral | LRU/LIRS/2Q | 3 | 0.5639 | 0.6167 | 4874283.1 | 512 | 2048 | 8192 |
| meta_reag.oracleGeneral | LRU/LIRS/2Q | 3 | 0.5769 | 0.6319 | 4729951.8 | 1024 | 4096 | 16384 |
| meta_reag.oracleGeneral | LRU/LIRS/2Q | 3 | 0.5910 | 0.6473 | 4577465.3 | 2048 | 8192 | 32768 |
| meta_reag.oracleGeneral | LRU/LIRS/2Q | 3 | 0.6066 | 0.6624 | 4405886.9 | 4096 | 16384 | 65536 |
| meta_reag.oracleGeneral | LRU/LIRS/2Q | 3 | 0.6223 | 0.6778 | 4233040.5 | 8192 | 32768 | 131072 |
| meta_reag.oracleGeneral | LRU/LIRS/2Q | 3 | 0.6397 | 0.6925 | 4043436.9 | 16384 | 65536 | 262144 |

L0: 2Q -> LRU. The workload may be a bit like `phase_shift`. On small capacities, 2Q can win by protecting keys seen twice; on large, there is no need for it because LRU is enough.

L1: ARC -> 2Q -> LIRS. L1 = 1024 is the point when LIRS is able to build a working LIR list for that workload. When < 1024, ARC or 2Q do better

L2: 2Q only. Keys left after LIRS/ARC L1 are cold. There are lot of them, but they are repeatable something like 2 times. 2Q with "seen once/seen twice" classification works better than others.

LFU is not used anywhere because of `phase_shift` structure mentioned: there might be a spike of requests to a certain key, after which it will be unused for a long time, and its high-frequency rating affects negatively.


#### 2 levels
Cache is 2-tier memory-like with parameters

```
-L0 <L0>   -T0 100'000
-L1 <L0>   -T1 1'000'000
--miss-ns 10'000'000
--overhead-ns 1000
```

effectively 3-tier case without L0.

Results:

| workload | algorithms | levels | hit_rate | opt_rate | sim_ns/op | L0 | L1 |
|:---------|:-----------|-------:|---------:|---------:|----------:|:--:|---:|
| meta_reag.oracleGeneral | ARC/2Q | 2 | 0.5251 | 0.5706 | 5362114.8 | 256 | 1024 |
| meta_reag.oracleGeneral | 2Q/2Q | 2 | 0.5385 | 0.5854 | 5215456.3 | 512 | 2048 |
| meta_reag.oracleGeneral | 2Q/2Q | 2 | 0.5510 | 0.6005 | 5074310.0 | 1024 | 4096 |
| meta_reag.oracleGeneral | 2Q/2Q | 2 | 0.5631 | 0.6156 | 4937314.9 | 2048 | 8192 |
| meta_reag.oracleGeneral | 2Q/2Q | 2 | 0.5759 | 0.6309 | 4795295.9 | 4096 | 16384 |
| meta_reag.oracleGeneral | 2Q/2Q | 2 | 0.5895 | 0.6462 | 4647500.7 | 8192 | 32768 |
| meta_reag.oracleGeneral | 2Q/2Q | 2 | 0.6052 | 0.6614 | 4477782.6 | 16384 | 65536 |
| meta_reag.oracleGeneral | 2Q/2Q | 2 | 0.6217 | 0.6767 | 4308391.7 | 32768 | 131072 |
| meta_reag.oracleGeneral | 2Q/2Q | 2 | 0.6377 | 0.6919 | 4125093.2 | 65536 | 262144 |

Almost all of positions are occupied by 2Q. The reason is that in 2-tier L0 is responsible for both "hot right now" and "hot in general" keys, and 2Q is the most suitable here. In 3-tier, those roles are divided between L0 and L1 accordingly, and they can be specialized better (LRU/LIRS).










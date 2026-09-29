# NHWC


An implementation of several cache replacement algorithms with a
multi-level system that combines them


## Overview


The project provides five cache replacement policies: LRU, LFU, ARC, 2Q and LIRS.


In addition, an `OptimalCache` implements Belady's algorithm — it knows
the entire future sequence in advance and gives the theoretical upper bound on
hit rate. It is not meant to be used inside a live cache system,only as a reference.


A `CacheSystem` holds a vector of levels (each one any of the algorithms above) and routes requests through
them. On a hit in a lower level, the entry is promoted upward via a cascading
insert; on eviction, the victim moves down. All levels share the same key/
value types.


Every algorithm inherits from `Cache<K, V>` and
  implements `contains`, `get`, `insert_ptr`, `extract_ptr`, and `dump`. The
  public `insert`/`extract` wrappers accept plain values; the `_ptr` variants
  work with `unique_ptr<const V>` for faster promotion between levels.


## Build


### Requirements

- CMake 3.24 or newer
- A C++20 compiler

GoogleTest is fetched automatically via CMake's
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
│   ├── lru_cache.h
│   ├── lru_cache.tpp
│   ├── lfu_cache.h
│   ├── lfu_cache.tpp
│   ├── lirs_cache.h
│   ├── lirs_cache.tpp
│   ├── arc_cache.h
│   ├── arc_cache.tpp
│   ├── twoq_cache.h
│   ├── twoq_cache.tpp
│   ├── opt_cache.h
│   ├── opt_cache.tpp
│   └── ghost_info.h
├── src/
│   └── main.cpp
├── bench/
│   ├── main.cpp
│   ├── runner.h
│   ├── runner.cpp
│   ├── search.h
│   ├── search.cpp
│   ├── workloads.h
│   └── workloads.cpp
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
**Note:** You may want to use `???Cache<K, V>` as a standalone class without cache system. If you do, you need to call `insert` manually in case of miss because 
`get` does not do it. Also some restrictions on `extract`, see `Methods`


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

**Note:** OptimalCache can't be used in cache system

**Note:** `simulate()` mutates the internal state and therefore must be called only once



## Methods


### CacheSystem<K, V> — construction and configuration


CacheSystem(std::function<V(const K&)> slow_get_page)

Creates an empty system. The argument is a function that will be called in case of miss




### CacheSystem<K, V>


#### `V access(const K& key)`

Returns the value for key, either from one of the levels (a hit) or from
the miss handler (a miss)



### Cache<K, V> — the common interface


Every algorithm implements this interface. Methods split into two groups:
the public value-based API and the internal pointer-based API used by
CacheSystem.


#### Value-based API


#### `std::optional<std::pair<K, V>> insert(const K& key, const V& value)`

Value based wrapper around `insert-ptr`


#### `std::optional<V> extract(const K& key)`

Value-based wrapper around `extract_ptr`

**Note:** using `extract` manually is not recommended on LIRS since it can affect LIR/HIR.


#### `std::optional<V> get(const K& key)`

Reads the value and updates the level's internal state


#### `bool contains(const K& key) const`

Checks whether the key is resident. Does not affect cache state


#### Pointer-based API


These methods exist so that CacheSystem can move values between levels
without copying V


#### `std::optional<std::pair<K, std::unique_ptr<const V>>> insert_ptr(const K& key, std::unique_ptr<const V> value)`

The value is passed and returned through
unique_ptr<const V>, so no copy of V is made


#### `std::unique_ptr<const V> extract_ptr(const K& key)`

Extracts an element from cache.

**Note:** using `extract_ptr` manually is not recommended on LIRS since it can affect LIR/HIR.


#### `void dump(std::ostream& out) const`

Prints a snapshot of the cache's internal structures.

**Note:** Since `OptimalCache` runs all sequence as a whole, the `dump` function makes sense only inside the its `simulate` loop



#### OptimalCache<K, V>


OptimalCache(std::size_t capacity, const std::vector<K>& sequence)

Stores the entire request sequence for offline analysis. Builds an index
from key to the list of positions where it appears. Requires
capacity >= 1; smaller values are rejected by validate_capacity


#### `std::size_t simulate()`

Runs over the stored sequence and returns the number of hits.



#### `void dump(std::ostream& out) const`

Prints the cache state for inspection





## CacheSystem


`CacheSystem<K, V>` is a container of cache levels. Each level is an
independent `Cache<K, V>` implementation (LRU, LFU, LIRS, ARC, or 2Q).
Levels are indexed from 0 (the topmost, usually smallest and fastest) to
`N−1` (the bottom-most, usually largest and slowest). All levels share the
same key and value types; internally every level stores values as
`unique_ptr<const V>`.


### Construction


The system is built incrementally via `add_cache`:


```cpp
CacheSystem<int, std::string> cs(slow_get_page);

cs.add_cache("LRU", 500);
cs.add_cache("LFU", 5000);
cs.add_cache("LIRS", 20000);
```



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
and restores them if an element is returned quickly enough. `SystemGhost` is LRU.






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
- `ext <key> >/ <value>` or `ext <key> >/ miss`.
- `cntn <key> >/ true` or `cntn <key> >/ false`.

Included files: `lru_sequence.txt`, `lfu_sequence.txt`,
`lirs_sequence.txt`, `arc_sequence.txt`, `twoq_sequence.txt`. Extra
scenarios can be supplied on the command line:

    ./build/test_cache file=path/to/my_sequence.txt

The `file=` prefix is repeatable. When no `file=` argument is given, the
built-in list above is used.

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

### Debugging a failure

`test_cache.cpp` wraps each step in `SCOPED_TRACE` with the file, step
index, operation and key, so the failing line is visible in the output.
On the first failing assertion the cache state is dumped to `stderr`




## Cache configuration search (cache_bench)

The project also has `cache_bench`, a driver that, given a workload and
a multi-level topology, searches for a good per-level algorithm assignment
using hill climbing

### Workloads

Workloads are described in a plain text file, one spec per line:

    <type> <name> key=value ...

Supported types:

- `uniform` — keys drawn uniformly from `[0, key_max)`.
- `zipf` — single Zipf distribution with exponent `alpha`.
- `zipfmulti` — alternating Zipf phases;
  parameters `alphas=0.7,1.1,1.5` and `ops_per_phase=N`.
- `hotcold` — mixture: `hot_ratio` of accesses go to one of `hot_keys`
  hot keys, the rest to one of `cold_keys` cold keys.
- `scan` - hot keys plus a sequential scan of
  `scan_length` keys.
- `phase` — `num_phases` phases; each phase works
  over `keys_per_phase` new keys.

Example workloads.txt file:

    uniform  u1    key_max=100000
    zipf     z1    key_max=100000 alpha=0.9
    hotcold  hc1   hot_ratio=0.8 hot_keys=100 cold_keys=100000
    scan     s1    hot_ratio=0.5 hot_keys=100 scan_length=1000
    phase    p1    phases=10 keys_per_phase=1000

### Objective

Each candidate configuration is evaluated by `run()` from `runner.cpp`.
It feeds the sequence through a `CacheSystem` and accumulates simulated
cost in nanoseconds:

- a hit at level `i` costs the prefix sum of latencies `T0..Ti`;
- a miss costs the sum of all level latencies plus `miss_ns`;
- every access adds `overhead_ns`.

The objective minimised by the search is `sim_ns_per_op`.

### Search

`hill_climb` explores the neighbourhood of the current
best configuration:

- replace one level's algorithm with any other from the pool;
- append a new level
- drop the last level

Neighbours are deduplicated by their string form. The search stops after
`--max-iter` iterations or after `no_improvement_stop` (currently fixed
to 1) non-improving iterations.

The benchmark restarts the hill climb from every algorithm in the pool
and keeps the best result per workload.

### CLI

    cache_bench --workloads PATH \
                -L0 <cap> [-L1 <cap> ...] \
                -T0 <ns>  [-T1 <ns>  ...] \
                --miss-ns <ns> [options]

Required:

- `--workloads PATH` — file with workload specs
- `-L{i} N` — capacity of level `i`
- `-T{i} N` — latency of level `i` in nanoseconds
- `--miss-ns N` — cost of a slow_get_page() access

Optional:

- `--overhead-ns N` — per-op overhead (default 20).
- `--max-iter N` — hill-climb iteration cap per restart (default 20).
- `--ops N` — sequence length (default 1 000 000).
- `--seed S` — RNG seed (default 1234).
- `--algorithm NAME` — restrict the algorithm pool (repeatable).
- `--csv PATH` — append the search log to a CSV file.

### Output

For every workload the tool prints the best configuration found, the
simulated cost, the achieved hit rate, the Belady optimum for the same
total capacity, and the relative gap. A summary table is printed at the
end. Example:
    
    workload      algorithms               levels   hit_rate   opt_rate   sim_ns/op
    u_15k         2Q/LRU/LIRS                   3     0.1405     0.4760    682647.6
    z_50k_09      LIRS/ARC/ARC                  3     0.5674     0.6675    374659.0
    z_swing       LIRS/ARC/ARC                  3     0.6610     0.7402    295797.8
    hc_oltp       LIRS/LFU/LFU                  3     0.1818     0.4423    659976.4
    scan_narrow   LFU/ARC                       2     0.4992     0.4992    485918.6
    scan_wide     LFU/LFU/LFU                   3     0.1826     0.4377    659584.5
    phase_3       2Q/LIRS/LRU                   3     0.4184     0.7530    529836.6






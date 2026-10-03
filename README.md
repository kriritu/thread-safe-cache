# Thread-Safe LRU & LFU Cache (C++17)

![ci](https://github.com/kriritu/thread-safe-cache/actions/workflows/ci.yml/badge.svg)

Header-only, from-scratch LRU and LFU caches with O(1) get/put/evict, made
thread-safe with `std::shared_mutex`, verified race-free with ThreadSanitizer,
and benchmarked at 1, 2, 4 and 8 threads.

## Design

- **LRU**: doubly linked list (`std::list`) + `unordered_map` from key to list
  iterator. `get` and `put` move the node to the front with `splice` (O(1), no
  iterator invalidation); eviction pops the back.
- **LFU**: `unordered_map<freq, list<Entry>>` buckets + `unordered_map` from key
  to entry iterator + `min_freq`. An access splices the entry from bucket f to
  f+1; eviction removes the least-recently-used entry of the lowest-frequency
  bucket (LeetCode 460 tie-break).
- **Thread safety** (`SharedMutexCache`): one `std::shared_mutex` per cache.
  `get`/`put` mutate recency or frequency state, so they take an exclusive
  lock. `peek`/`size`/`contains` do not mutate and take a shared lock.
- **Sharding** (`ShardedCache`): N independent shards, each with its own
  lock, keyed by a mixed hash. Threads on different shards never contend. The
  trade-off is that eviction order is per shard, not global.

## Build and test

    cmake --preset debug && cmake --build --preset debug && ctest --preset debug
    cmake --preset asan  && cmake --build --preset asan  && ctest --preset asan
    cmake --preset tsan  && cmake --build --preset tsan  && ctest --preset tsan

Run everything plus the benchmark:

    scripts/run_all.sh

## Correctness

- LeetCode 146 (LRU) and 460 (LFU) sequences.
- Randomized differential tests against simple O(n) reference models.
- Stress test: 8 threads x 100k random get/put/peek/size operations, checking
  value integrity (every value must equal f(key)), size bounds and final
  contents. It runs under ThreadSanitizer with zero reported data races.
- `tests/tsan_race_demo.cpp` deliberately takes a shared lock in `get()`;
  ThreadSanitizer reports the race, which confirms the detector is active.

## Benchmarks

Release build, 80% get / 20% put, uniform random keys, capacity 4096.
Throughput in million ops/sec, median of 3 runs.

    (paste the table from docs/benchmark-results.txt here)

Machine: (CPU model, cores, WSL2/Ubuntu version, GCC version)

## Layout

    include/cache/lru_cache.hpp         single-threaded LRU
    include/cache/lfu_cache.hpp         single-threaded LFU
    include/cache/concurrent_cache.hpp  shared_mutex wrapper
    include/cache/sharded_cache.hpp     sharded wrapper
    tests/                              unit, differential, stress, TSan demo
    bench/bench_cache.cpp               throughput benchmark

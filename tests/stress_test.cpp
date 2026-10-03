#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <thread>
#include <vector>

#include "cache/concurrent_cache.hpp"
#include "cache/sharded_cache.hpp"

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "CHECK failed: " #cond " at " << __FILE__ << ":"  \
                      << __LINE__ << "\n";                                 \
            std::abort();                                                  \
        }                                                                  \
    } while (0)

namespace {

int value_of(int k) { return k * 31 + 7; }

struct alignas(64) Stats {
    std::uint64_t gets = 0, hits = 0, puts = 0, peeks = 0, sizes = 0;
};

// Single-threaded sanity: the locking wrapper must behave exactly like the
// unsynchronised cache it wraps.
template <template <typename, typename> class Impl>
void test_wrapper_matches_plain(std::size_t cap, int range, int ops, std::uint32_t seed) {
    Impl<int, int> plain(cap);
    cache::SharedMutexCache<int, int, Impl> wrapped(cap);
    std::mt19937 rng(seed);
    for (int i = 0; i < ops; ++i) {
        int k = static_cast<int>(rng() % static_cast<std::uint32_t>(range));
        switch (rng() % 3) {
            case 0: {
                int v = static_cast<int>(rng() % 100000);
                plain.put(k, v);
                wrapped.put(k, v);
                break;
            }
            case 1:
                CHECK(plain.get(k) == wrapped.get(k));
                break;
            default:
                CHECK(plain.peek(k) == wrapped.peek(k));
                break;
        }
        CHECK(plain.size() == wrapped.size());
    }
}

template <typename CacheT>
void stress(const char* name, CacheT& c, int threads, int ops, int key_range,
            std::uint32_t seed) {
    std::vector<Stats> stats(static_cast<std::size_t>(threads));
    std::atomic<bool> go{false};
    std::vector<std::thread> pool;

    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&, t] {
            std::mt19937 rng(seed + static_cast<std::uint32_t>(t));
            Stats& s = stats[static_cast<std::size_t>(t)];
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int i = 0; i < ops; ++i) {
                int k = static_cast<int>(rng() % static_cast<std::uint32_t>(key_range));
                unsigned r = rng() % 100;
                if (r < 45) {
                    ++s.gets;
                    auto v = c.get(k);
                    if (v) {
                        ++s.hits;
                        CHECK(*v == value_of(k));
                    }
                } else if (r < 80) {
                    c.put(k, value_of(k));
                    ++s.puts;
                } else if (r < 95) {
                    ++s.peeks;
                    auto v = c.peek(k);
                    if (v) CHECK(*v == value_of(k));
                } else {
                    ++s.sizes;
                    CHECK(c.size() <= c.capacity());
                }
            }
        });
    }

    go.store(true, std::memory_order_release);
    for (auto& th : pool) th.join();

    Stats total;
    for (const auto& s : stats) {
        total.gets += s.gets;
        total.hits += s.hits;
        total.puts += s.puts;
        total.peeks += s.peeks;
        total.sizes += s.sizes;
    }
    CHECK(total.gets + total.puts + total.peeks + total.sizes ==
          static_cast<std::uint64_t>(threads) * static_cast<std::uint64_t>(ops));
    CHECK(total.hits <= total.gets);

    // Quiescent invariants: bounded size, no corrupted values, map == contents.
    CHECK(c.size() <= c.capacity());
    std::size_t present = 0;
    for (int k = 0; k < key_range; ++k) {
        auto v = c.peek(k);
        if (v) {
            ++present;
            CHECK(*v == value_of(k));
        }
    }
    CHECK(present == c.size());

    std::cout << "  " << name << ": " << threads << " threads x " << ops
              << " ops OK (gets=" << total.gets << " hits=" << total.hits
              << " puts=" << total.puts << " size=" << c.size() << "/"
              << c.capacity() << ")\n";
}

struct Config {
    std::size_t capacity;
    int key_range;
};

}  // namespace

int main() {
    constexpr int kThreads = 8;
    constexpr int kOps = 100000;

    test_wrapper_matches_plain<cache::LRUCache>(50, 120, 100000, 1);
    test_wrapper_matches_plain<cache::LFUCache>(50, 120, 100000, 2);
    std::cout << "wrapper matches unsynchronised cache: OK\n";

    // Config 1: moderate contention, lots of evictions.
    // Config 2: tiny key space and capacity = maximum contention on few nodes.
    for (const Config cfg : {Config{256, 1024}, Config{16, 64}}) {
        std::cout << "capacity=" << cfg.capacity << " key_range=" << cfg.key_range << "\n";
        {
            cache::ThreadSafeLRUCache<int, int> c(cfg.capacity);
            stress("ThreadSafeLRU   ", c, kThreads, kOps, cfg.key_range, 100);
        }
        {
            cache::ThreadSafeLFUCache<int, int> c(cfg.capacity);
            stress("ThreadSafeLFU   ", c, kThreads, kOps, cfg.key_range, 200);
        }
        {
            cache::ShardedLRUCache<int, int> c(cfg.capacity, 8);
            stress("ShardedLRU(8)   ", c, kThreads, kOps, cfg.key_range, 300);
        }
        {
            cache::ShardedLFUCache<int, int> c(cfg.capacity, 8);
            stress("ShardedLFU(8)   ", c, kThreads, kOps, cfg.key_range, 400);
        }
    }

    std::cout << "stress_test: all tests passed\n";
    return 0;
}

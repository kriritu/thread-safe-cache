// INTENTIONALLY BROKEN. Not part of ctest.
// get() takes a SHARED lock but LFUCache::get() mutates state (frequency
// counters, buckets), so concurrent gets are a data race. Run ONLY in the
// tsan build:
//   cmake --build --preset tsan --target tsan_race_demo
//   TSAN_OPTIONS="halt_on_error=1" ./build/tsan/tests/tsan_race_demo
// ThreadSanitizer should print "WARNING: ThreadSanitizer: data race".
#include <iostream>
#include <mutex>
#include <optional>
#include <random>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "cache/lfu_cache.hpp"

struct BrokenCache {
    mutable std::shared_mutex mu;
    cache::LFUCache<int, int> impl{8};

    std::optional<int> get(int k) {
        std::shared_lock<std::shared_mutex> lock(mu);  // BUG: get() mutates state
        return impl.get(k);
    }
    void put(int k, int v) {
        std::unique_lock<std::shared_mutex> lock(mu);
        impl.put(k, v);
    }
};

int main() {
    BrokenCache c;
    for (int k = 0; k < 8; ++k) c.put(k, k);

    std::vector<std::thread> pool;
    for (int t = 0; t < 2; ++t) {
        pool.emplace_back([&c, t] {
            std::mt19937 rng(static_cast<unsigned>(t));
            for (int i = 0; i < 50000; ++i) c.get(static_cast<int>(rng() % 8));
        });
    }
    for (auto& th : pool) th.join();
    std::cerr << "finished without a TSan report (TSan not active?)\n";
    return 0;
}

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "cache/concurrent_cache.hpp"
#include "cache/sharded_cache.hpp"

namespace {

constexpr std::size_t kCapacity = 4096;
constexpr std::uint32_t kKeyRange = 8192;  // 2x capacity -> roughly 50% hit rate
constexpr unsigned kPutPercent = 20;       // 80% get / 20% put
constexpr int kRepeats = 3;

struct Op {
    std::uint32_t key;
    bool is_put;
};

struct alignas(64) Sink {
    std::uint64_t value = 0;
};

volatile std::uint64_t g_sink = 0;  // keeps the compiler from deleting the work

template <typename CacheT>
double run_once(CacheT& cache, int threads, std::size_t ops_per_thread) {
    for (std::uint32_t k = 0; k < kCapacity; ++k) cache.put(k, k);  // warm up

    std::vector<std::vector<Op>> ops(static_cast<std::size_t>(threads));
    for (int t = 0; t < threads; ++t) {
        std::mt19937 rng(1234u + static_cast<unsigned>(t));
        auto& v = ops[static_cast<std::size_t>(t)];
        v.reserve(ops_per_thread);
        for (std::size_t i = 0; i < ops_per_thread; ++i) {
            v.push_back(Op{rng() % kKeyRange, (rng() % 100) < kPutPercent});
        }
    }

    std::vector<Sink> sinks(static_cast<std::size_t>(threads));
    std::atomic<bool> go{false};
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&, t] {
            const auto& my_ops = ops[static_cast<std::size_t>(t)];
            std::uint64_t sink = 0;
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            for (const Op& op : my_ops) {
                if (op.is_put) {
                    cache.put(op.key, op.key);
                } else if (auto v = cache.get(op.key)) {
                    sink += *v;
                }
            }
            sinks[static_cast<std::size_t>(t)].value = sink;
        });
    }

    const auto t0 = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    for (auto& th : pool) th.join();
    const auto t1 = std::chrono::steady_clock::now();

    std::uint64_t total_sink = 0;
    for (const auto& s : sinks) total_sink += s.value;
    g_sink = total_sink;

    const double secs = std::chrono::duration<double>(t1 - t0).count();
    return static_cast<double>(ops_per_thread) * threads / secs;  // ops/sec
}

template <typename CacheT, typename... Args>
double median_throughput(int threads, std::size_t ops_per_thread, Args... args) {
    std::vector<double> results;
    for (int r = 0; r < kRepeats; ++r) {
        auto cache = std::make_unique<CacheT>(args...);
        results.push_back(run_once(*cache, threads, ops_per_thread));
    }
    std::sort(results.begin(), results.end());
    return results[results.size() / 2];
}

template <typename CacheT, typename... Args>
void row(const std::string& name, std::size_t ops_per_thread, Args... args) {
    std::cout << "| " << std::left << std::setw(26) << name << " |";
    for (int threads : {1, 2, 4, 8}) {
        const double mops = median_throughput<CacheT>(threads, ops_per_thread, args...) / 1e6;
        std::cout << " " << std::right << std::setw(8) << std::fixed << std::setprecision(2)
                  << mops << " |";
        std::cout.flush();
    }
    std::cout << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    const std::size_t ops_per_thread =
        argc > 1 ? static_cast<std::size_t>(std::stoull(argv[1])) : 500000;

    std::cout << "hardware threads: " << std::thread::hardware_concurrency()
              << " | capacity=" << kCapacity << " keys=" << kKeyRange
              << " mix=" << (100 - kPutPercent) << "% get/" << kPutPercent << "% put"
              << " | ops/thread=" << ops_per_thread << " | median of " << kRepeats << "\n\n";
    std::cout << "Throughput in million ops/sec (higher is better)\n\n";
    std::cout << "| Implementation             |  1 thr   |  2 thr   |  4 thr   |  8 thr   |\n";
    std::cout << "|----------------------------|----------|----------|----------|----------|\n";

    using K = std::uint32_t;
    row<cache::ThreadSafeLRUCache<K, K>>("LRU shared_mutex", ops_per_thread, kCapacity);
    row<cache::ThreadSafeLFUCache<K, K>>("LFU shared_mutex", ops_per_thread, kCapacity);
    row<cache::ShardedLRUCache<K, K>>("LRU sharded (16)", ops_per_thread, kCapacity,
                                      std::size_t{16});
    row<cache::ShardedLFUCache<K, K>>("LFU sharded (16)", ops_per_thread, kCapacity,
                                      std::size_t{16});

    std::cout << "\nTarget: >= 1.00 Mops/s\n";
    return 0;
}

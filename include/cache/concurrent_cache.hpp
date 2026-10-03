#pragma once
#include <cstddef>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <utility>

#include "cache/lfu_cache.hpp"
#include "cache/lru_cache.hpp"

namespace cache {

// Thread-safe wrapper around any single-threaded cache (LRUCache, LFUCache).
//
// Locking rules:
//   get()/put()       mutate internal state (recency list / frequency buckets),
//                     so they take an EXCLUSIVE lock.
//   peek()/size()/... do not mutate, so they take a SHARED lock and many
//                     of them can run in parallel.
//
// There is exactly one mutex and it is never held while calling user code,
// so deadlock is impossible.
template <typename K, typename V, template <typename, typename> class Impl>
class SharedMutexCache {
public:
    explicit SharedMutexCache(std::size_t capacity) : impl_(capacity) {}

    SharedMutexCache(const SharedMutexCache&) = delete;
    SharedMutexCache& operator=(const SharedMutexCache&) = delete;

    std::optional<V> get(const K& key) {
        std::unique_lock<std::shared_mutex> lock(mu_);
        return impl_.get(key);
    }

    void put(const K& key, V value) {
        std::unique_lock<std::shared_mutex> lock(mu_);
        impl_.put(key, std::move(value));
    }

    std::optional<V> peek(const K& key) const {
        std::shared_lock<std::shared_mutex> lock(mu_);
        return impl_.peek(key);
    }

    bool contains(const K& key) const { return peek(key).has_value(); }

    std::size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mu_);
        return impl_.size();
    }

    // Capacity never changes after construction, so reading it needs no lock.
    std::size_t capacity() const { return impl_.capacity(); }

private:
    mutable std::shared_mutex mu_;
    Impl<K, V> impl_;
};

template <typename K, typename V>
using ThreadSafeLRUCache = SharedMutexCache<K, V, LRUCache>;

template <typename K, typename V>
using ThreadSafeLFUCache = SharedMutexCache<K, V, LFUCache>;

}  // namespace cache

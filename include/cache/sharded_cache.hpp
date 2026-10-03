#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "cache/concurrent_cache.hpp"

namespace cache {

// N independent shards, each a SharedMutexCache with its own lock.
// A key always maps to the same shard, so threads touching different shards
// never contend. Eviction order is per-shard (approximate globally).
//
// Total capacity is rounded up to a multiple of the shard count.
template <typename K, typename V, template <typename, typename> class Impl,
          typename Hash = std::hash<K>>
class ShardedCache {
public:
    explicit ShardedCache(std::size_t capacity, std::size_t shards = 16) {
        if (shards == 0) shards = 1;
        if (capacity > 0 && shards > capacity) shards = capacity;
        const std::size_t per_shard = capacity == 0 ? 0 : (capacity + shards - 1) / shards;
        shards_.reserve(shards);
        for (std::size_t i = 0; i < shards; ++i) {
            shards_.push_back(std::make_unique<Shard>(per_shard));
        }
        capacity_ = per_shard * shards;
    }

    std::optional<V> get(const K& key) { return shard_for(key).cache.get(key); }

    void put(const K& key, V value) { shard_for(key).cache.put(key, std::move(value)); }

    std::optional<V> peek(const K& key) const { return shard_for(key).cache.peek(key); }

    bool contains(const K& key) const { return peek(key).has_value(); }

    // Locks one shard at a time, so under concurrent writes this is not an
    // atomic snapshot (but it never exceeds capacity()).
    std::size_t size() const {
        std::size_t total = 0;
        for (const auto& s : shards_) total += s->cache.size();
        return total;
    }

    std::size_t capacity() const { return capacity_; }
    std::size_t shard_count() const { return shards_.size(); }

private:
    // alignas(64): each shard (and its mutex) sits on its own cache line(s),
    // avoiding false sharing between neighbouring shards.
    struct alignas(64) Shard {
        explicit Shard(std::size_t cap) : cache(cap) {}
        SharedMutexCache<K, V, Impl> cache;
    };

    // std::hash<int> is the identity function; mix the bits so sequential
    // keys spread across shards.
    Shard& shard_for(const K& key) const {
        std::size_t h = Hash{}(key);
        h ^= h >> 33;
        h *= static_cast<std::size_t>(0xff51afd7ed558ccdULL);
        h ^= h >> 33;
        return *shards_[h % shards_.size()];
    }

    std::vector<std::unique_ptr<Shard>> shards_;
    std::size_t capacity_ = 0;
};

template <typename K, typename V>
using ShardedLRUCache = ShardedCache<K, V, LRUCache>;

template <typename K, typename V>
using ShardedLFUCache = ShardedCache<K, V, LFUCache>;

}  // namespace cache

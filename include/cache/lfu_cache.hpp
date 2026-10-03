#pragma once
#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

namespace cache {

// Single-threaded LFU cache with O(1) get/put/evict.
// Ties between equal frequencies are broken by least-recently-used.
//
// Invariants (true after every public call):
//   1. index_.size() <= cap_, and equals the total entries across all buckets
//   2. every key in index_ points to its entry, which lives in buckets_[entry.freq]
//   3. no empty bucket is stored in buckets_
//   4. min_freq_ is the smallest key of buckets_ whenever the cache is non-empty
template <typename K, typename V>
class LFUCache {
public:
    explicit LFUCache(std::size_t capacity) : cap_(capacity) {}

    // Returns the value and counts the access.
    std::optional<V> get(const K& key) {
        auto it = index_.find(key);
        if (it == index_.end()) return std::nullopt;
        touch(it->second);
        return it->second->value;
    }

    // Insert or update. Updating an existing key counts as an access.
    void put(const K& key, V value) {
        if (cap_ == 0) return;

        auto it = index_.find(key);
        if (it != index_.end()) {
            it->second->value = std::move(value);
            touch(it->second);
            return;
        }

        if (index_.size() >= cap_) evict();

        auto& bucket = buckets_[1];
        bucket.push_front(Entry{key, std::move(value), 1});
        index_.emplace(key, bucket.begin());
        min_freq_ = 1;
    }

    // Does NOT count as an access. Const, so a reader lock can protect it later.
    std::optional<V> peek(const K& key) const {
        auto it = index_.find(key);
        if (it == index_.end()) return std::nullopt;
        return it->second->value;
    }

    // Current access count of a key (for tests/debugging). Does not count as access.
    std::optional<std::size_t> frequency(const K& key) const {
        auto it = index_.find(key);
        if (it == index_.end()) return std::nullopt;
        return it->second->freq;
    }

    std::size_t size() const { return index_.size(); }
    std::size_t capacity() const { return cap_; }

private:
    struct Entry {
        K key;
        V value;
        std::size_t freq;
    };
    using Bucket = std::list<Entry>;
    using BucketIt = typename Bucket::iterator;

    // Move an entry from bucket f to bucket f+1 (front = most recent).
    void touch(BucketIt it) {
        const std::size_t f = it->freq;
        Bucket& from = buckets_[f];
        Bucket& to = buckets_[f + 1];  // references into unordered_map stay valid on rehash

        // splice relinks the node without copying; `it` (and the iterator
        // stored in index_) keeps pointing at the same entry.
        to.splice(to.begin(), from, it);
        it->freq = f + 1;

        if (from.empty()) {
            buckets_.erase(f);
            if (min_freq_ == f) min_freq_ = f + 1;
        }
    }

    // Remove the LRU entry of the lowest-frequency bucket.
    void evict() {
        auto bit = buckets_.find(min_freq_);
        Bucket& bucket = bit->second;

        index_.erase(bucket.back().key);  // map first, while the entry still exists
        bucket.pop_back();
        if (bucket.empty()) buckets_.erase(bit);
    }

    std::size_t cap_;
    std::size_t min_freq_ = 0;
    std::unordered_map<std::size_t, Bucket> buckets_;   // freq -> entries
    std::unordered_map<K, BucketIt> index_;             // key -> entry
};

}  // namespace cache
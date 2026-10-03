#pragma once
#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

namespace cache {

// Single-threaded LRU cache with O(1) get/put/evict.
//
// Invariants (true after every public call):
//   1. items_.size() == index_.size() <= cap_
//   2. index_[k] is an iterator to the node in items_ whose key is k
//   3. items_.front() is the most recently used, items_.back() the least
template <typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(std::size_t capacity) : cap_(capacity) {}

    // Returns the value and promotes the key to most-recently-used.
    std::optional<V> get(const K& key) {
        auto it = index_.find(key);
        if (it == index_.end()) return std::nullopt;

        // Move the node to the front. splice relinks the node in O(1) and
        // does not invalidate any iterator, so it->second stays valid.
        items_.splice(items_.begin(), items_, it->second);
        return it->second->second;
    }

    // Insert or update; promote to MRU; evict the LRU entry if full.
    void put(const K& key, V value) {
        if (cap_ == 0) return;  // capacity 0: nothing can be stored

        auto it = index_.find(key);
        if (it != index_.end()) {
            // Existing key: update value and promote. No new node, no eviction.
            it->second->second = std::move(value);
            items_.splice(items_.begin(), items_, it->second);
            return;
        }

        // New key: make room first, then insert at the front.
        if (index_.size() >= cap_) evict_lru();

        items_.emplace_front(key, std::move(value));
        try {
            index_.emplace(key, items_.begin());
        } catch (...) {
            items_.pop_front();  // keep list and map in sync if emplace throws
            throw;
        }
    }

    // Does NOT change recency. Const, so a reader lock can protect it later.
    std::optional<V> peek(const K& key) const {
        auto it = index_.find(key);
        if (it == index_.end()) return std::nullopt;
        return it->second->second;
    }

    std::size_t size() const { return index_.size(); }
    std::size_t capacity() const { return cap_; }

private:
    using Node = std::pair<K, V>;
    using ListIt = typename std::list<Node>::iterator;

    // Remove the least-recently-used entry from BOTH containers.
    void evict_lru() {
        // Erase from the map first, while the node (and its key) still exists.
        // Doing pop_back() first would leave us reading a destroyed node.
        index_.erase(items_.back().first);
        items_.pop_back();
    }

    std::size_t cap_;
    std::list<Node> items_;                 // front = MRU, back = LRU
    std::unordered_map<K, ListIt> index_;   // key -> node in items_
};

}  // namespace cache
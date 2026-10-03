#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "cache/lru_cache.hpp"

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "CHECK failed: " #cond " at " << __FILE__ << ":"  \
                      << __LINE__ << "\n";                                 \
            std::abort();                                                  \
        }                                                                  \
    } while (0)

// Slow but obviously-correct reference. back() = most recently used.
template <typename K, typename V>
class RefLRU {
public:
    explicit RefLRU(std::size_t cap) : cap_(cap) {}

    std::optional<V> get(const K& k) {
        for (std::size_t i = 0; i < v_.size(); ++i) {
            if (v_[i].first == k) {
                auto e = v_[i];
                v_.erase(v_.begin() + static_cast<std::ptrdiff_t>(i));
                v_.push_back(e);
                return e.second;
            }
        }
        return std::nullopt;
    }

    void put(const K& k, V val) {
        if (cap_ == 0) return;  // capacity 0 = no-op; matches LRUCache
        for (std::size_t i = 0; i < v_.size(); ++i) {
            if (v_[i].first == k) {
                v_.erase(v_.begin() + static_cast<std::ptrdiff_t>(i));
                v_.push_back({k, val});
                return;
            }
        }
        if (v_.size() == cap_) v_.erase(v_.begin());
        v_.push_back({k, val});
    }

    std::size_t size() const { return v_.size(); }

private:
    std::size_t cap_;
    std::vector<std::pair<K, V>> v_;
};

void test_leetcode_146() {
    cache::LRUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    CHECK(c.get(1) == 1);
    c.put(3, 3);               // evicts key 2
    CHECK(!c.get(2).has_value());
    c.put(4, 4);               // evicts key 1
    CHECK(!c.get(1).has_value());
    CHECK(c.get(3) == 3);
    CHECK(c.get(4) == 4);
}

void test_update_existing_key() {
    cache::LRUCache<int, int> c(2);
    c.put(1, 10);
    c.put(2, 20);
    c.put(1, 11);              // update + promote, no eviction
    CHECK(c.size() == 2);
    c.put(3, 30);              // should evict 2, not 1
    CHECK(c.get(1) == 11);
    CHECK(!c.get(2).has_value());
}

void test_capacity_one() {
    cache::LRUCache<int, int> c(1);
    c.put(1, 1);
    c.put(2, 2);
    CHECK(!c.get(1).has_value());
    CHECK(c.get(2) == 2);
    CHECK(c.size() == 1);
}

void test_peek_does_not_promote() {
    cache::LRUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    CHECK(c.peek(1) == 1);     // must NOT make 1 the MRU
    c.put(3, 3);               // so 1 is still the LRU and gets evicted
    CHECK(!c.peek(1).has_value());
    CHECK(c.peek(2) == 2);
}

void test_differential(std::size_t cap, int key_range, int ops, std::uint32_t seed) {
    std::mt19937 rng(seed);
    cache::LRUCache<int, int> c(cap);
    RefLRU<int, int> r(cap);
    for (int i = 0; i < ops; ++i) {
        int k = static_cast<int>(rng() % static_cast<std::uint32_t>(key_range));
        if (rng() % 2) {
            int v = static_cast<int>(rng() % 1000000);
            c.put(k, v);
            r.put(k, v);
        } else {
            CHECK(c.get(k) == r.get(k));
        }
        CHECK(c.size() == r.size());
        CHECK(c.size() <= cap);
    }
}

int main() {
    test_leetcode_146();
    test_update_existing_key();
    test_capacity_one();
    test_peek_does_not_promote();
    for (std::size_t cap : {1u, 2u, 5u, 50u}) {
        test_differential(cap, static_cast<int>(cap) * 2 + 1, 100000,
                          12345u + static_cast<std::uint32_t>(cap));
    }
    std::cout << "test_lru: all tests passed\n";
    return 0;
}
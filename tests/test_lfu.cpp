#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "cache/lfu_cache.hpp"

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "CHECK failed: " #cond " at " << __FILE__ << ":"  \
                      << __LINE__ << "\n";                                 \
            std::abort();                                                  \
        }                                                                  \
    } while (0)

// Slow reference: evict min frequency, ties -> oldest last-use stamp.
template <typename K, typename V>
class RefLFU {
public:
    explicit RefLFU(std::size_t cap) : cap_(cap) {}

    std::optional<V> get(const K& k) {
        for (auto& e : v_) {
            if (e.k == k) {
                ++e.f;
                e.t = ++clock_;
                return e.v;
            }
        }
        return std::nullopt;
    }

    void put(const K& k, V val) {
        if (cap_ == 0) return;
        for (auto& e : v_) {
            if (e.k == k) {
                e.v = val;
                ++e.f;
                e.t = ++clock_;
                return;
            }
        }
        if (v_.size() == cap_) {
            std::size_t victim = 0;
            for (std::size_t i = 1; i < v_.size(); ++i) {
                if (v_[i].f < v_[victim].f ||
                    (v_[i].f == v_[victim].f && v_[i].t < v_[victim].t)) {
                    victim = i;
                }
            }
            v_.erase(v_.begin() + static_cast<std::ptrdiff_t>(victim));
        }
        v_.push_back(E{k, val, 1, ++clock_});
    }

    std::size_t size() const { return v_.size(); }

private:
    struct E {
        K k;
        V v;
        std::size_t f;
        std::uint64_t t;
    };
    std::size_t cap_;
    std::uint64_t clock_ = 0;
    std::vector<E> v_;
};

void test_leetcode_460() {
    cache::LFUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    CHECK(c.get(1) == 1);              // freq(1)=2
    c.put(3, 3);                       // evicts 2 (lowest freq)
    CHECK(!c.get(2).has_value());
    CHECK(c.get(3) == 3);              // freq(3)=2
    c.put(4, 4);                       // 1 and 3 tie at freq 2; 1 is older -> evict 1
    CHECK(!c.get(1).has_value());
    CHECK(c.get(3) == 3);
    CHECK(c.get(4) == 4);
}

void test_tie_break_is_lru() {
    cache::LFUCache<int, int> c(3);
    c.put(1, 1);
    c.put(2, 2);
    c.put(3, 3);                       // all freq 1; order oldest->newest: 1,2,3
    c.put(4, 4);                       // evicts 1
    CHECK(!c.peek(1).has_value());
    CHECK(c.peek(2).has_value());
    c.put(5, 5);                       // evicts 2
    CHECK(!c.peek(2).has_value());
}

void test_put_existing_counts_as_access() {
    cache::LFUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    c.put(1, 10);                      // freq(1)=2, value updated
    CHECK(c.frequency(1) == 2);
    c.put(3, 3);                       // evicts 2 (freq 1)
    CHECK(c.get(1) == 10);
    CHECK(!c.peek(2).has_value());
}

void test_peek_does_not_count() {
    cache::LFUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    CHECK(c.peek(1) == 1);
    CHECK(c.frequency(1) == 1);        // unchanged
    c.put(3, 3);                       // 1 is still oldest at freq 1 -> evicted
    CHECK(!c.peek(1).has_value());
}

void test_capacity_edges() {
    cache::LFUCache<int, int> zero(0);
    zero.put(1, 1);
    CHECK(zero.size() == 0);
    CHECK(!zero.get(1).has_value());

    cache::LFUCache<int, int> one(1);
    one.put(1, 1);
    one.put(2, 2);
    CHECK(!one.get(1).has_value());
    CHECK(one.get(2) == 2);
    CHECK(one.size() == 1);
}

void test_differential(std::size_t cap, int key_range, int ops, std::uint32_t seed) {
    std::mt19937 rng(seed);
    cache::LFUCache<int, int> c(cap);
    RefLFU<int, int> r(cap);
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
    test_leetcode_460();
    test_tie_break_is_lru();
    test_put_existing_counts_as_access();
    test_peek_does_not_count();
    test_capacity_edges();
    for (std::size_t cap : {1u, 2u, 5u, 50u}) {
        test_differential(cap, static_cast<int>(cap) * 2 + 1, 100000,
                          777u + static_cast<std::uint32_t>(cap));
    }
    std::cout << "test_lfu: all tests passed\n";
    return 0;
}
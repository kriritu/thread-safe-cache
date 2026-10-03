# Thread-Safe LRU & LFU Cache (C++17)

Header-only, from-scratch LRU and LFU caches with O(1) get/put,
made thread-safe with std::shared_mutex and verified with ThreadSanitizer.

## Build and test

    cmake --preset debug
    cmake --build --preset debug
    ctest --preset debug

## Sanitizers

    cmake --preset tsan && cmake --build --preset tsan && ctest --preset tsan

## Status
Work in progress (Day 0: project skeleton).

#!/usr/bin/env bash
# Runs every test configuration, then the release benchmark.
set -euo pipefail
cd "$(dirname "$0")/.."
for p in debug asan tsan; do
  echo "=== $p ==="
  cmake --preset "$p" > /dev/null
  cmake --build --preset "$p" -j
  ctest --preset "$p"
done
echo "=== release benchmark ==="
cmake --preset release > /dev/null
cmake --build --preset release -j
./build/release/bench/bench_cache | tee docs/benchmark-results.txt

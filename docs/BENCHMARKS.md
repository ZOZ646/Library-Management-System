# Benchmark Results: titans::Container vs std::list

This document reports empirical performance measurements comparing `titans::Container<int>` (a custom doubly linked list) against `std::list<int>` from the standard C++ library.

## Benchmark Execution

The benchmark executable is built from `bench/bench_container.cpp`. It is disabled by default and built in an isolated directory:

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DTITANS_SANITIZE=OFF -DTITANS_BUILD_BENCH=ON
cmake --build build-bench --config Release --target titans_bench
./build-bench/titans_bench
```

> **Note:** Timings are machine-dependent and subject to OS scheduling. The benchmark is not part of the automated unit test suite.

## Hardware and Environment

- **Operating System:** Windows 11 Pro (Build 10.0.26100)
- **CPU:** 12th Gen Intel(R) Core(TM) i5-12400F (6 Cores / 12 Logical Processors @ 2.50 GHz base)
- **Compiler:** Microsoft Visual C++ Compiler (MSVC 19.50.35730.0 / Visual Studio 2026)
- **C++ Standard:** C++17
- **Build Type:** Release (Optimizations enabled, AddressSanitizer disabled)

## Methodology

- Fixed random seed: `std::mt19937 rng(12345)`
- Tested sizes: $n \in \{1000, 10000, 100000\}$
- Measurement metric: Median of 7 independent runs using `std::chrono::steady_clock`
- Timing units: Milliseconds (ms) with 3 decimals (except `find`, which reports microseconds per call, $\mu\text{s/call}$)
- Optimization barrier: Results accumulated into `volatile long long g_sink` to prevent dead-code elimination

## Results

```
Compiler: MSVC 1950
Build type: Release
n = 1000, 10000, 100000; median of 7 runs
```

### push_back (ms)

| n | titans::Container<int> | std::list<int> |
|---|---|---|
| 1000 | 0.028 | 0.022 |
| 10000 | 0.251 | 0.250 |
| 100000 | 2.541 | 2.797 |

### Iterate and sum (ms)

| n | titans::Container<int> | std::list<int> |
|---|---|---|
| 1000 | 0.003 | 0.002 |
| 10000 | 0.028 | 0.023 |
| 100000 | 0.410 | 0.377 |

### find last element (us/call)

| n | titans::Container<int> | std::list<int> |
|---|---|---|
| 1000 | 1.154 | 1.152 |
| 10000 | 23.230 | 21.909 |
| 100000 | 283.345 | 280.815 |

### Sort random elements (ms)

| n | titans::Container<int>::sort() | std::list<int>::sort() |
|---|---|---|
| 1000 | 0.046 | 0.041 |
| 10000 | 0.669 | 0.560 |
| 100000 | 11.416 | 8.444 |

### titans::Container<int>::insertion_sort() (ms)

| Input condition | n | titans::Container<int>::insertion_sort() |
|---|---|---|
| Random (O(n^2)) | 1000 | 0.304 |
| Random (O(n^2)) | 10000 | 88.228 |
| Sorted (O(n) best case) | 100000 | 0.367 |

### remove_value of every 2nd element (ms)

| n | titans::Container<int>::remove_value | std::list<int>::remove |
|---|---|---|
| 10000 | 37.851 | 91.625 |

## Run-to-Run Variation

The benchmark was executed across two distinct complete runs to evaluate stability:
- `push_back` at $n = 100\,000$: Run 1 was 2.541 ms; Run 2 was 2.848 ms ($\approx 11\%$ variation).
- Iteration sum at $n = 100\,000$: Run 1 was 0.410 ms; Run 2 was 0.416 ms ($< 1.5\%$ variation).
- `find` last element at $n = 100\,000$: Run 1 was 283.345 $\mu\text{s}$; Run 2 was 289.440 $\mu\text{s}$ ($\approx 2\%$ variation).
- `sort()` at $n = 100\,000$: Run 1 was 11.416 ms; Run 2 was 12.050 ms ($\approx 5\%$ variation).
- `insertion_sort()` at $n = 10\,000$ (random): Run 1 was 88.228 ms; Run 2 was 88.239 ms ($< 0.1\%$ variation).
- `remove_value` at $n = 10\,000$: Run 1 was 37.851 ms; Run 2 was 38.051 ms ($< 0.6\%$ variation).

Overall variation across medians remained below 5-10%, reflecting steady CPU execution.

## What This Shows

1. **Memory Allocation & Traversal Overhead:**
   Both `titans::Container` and `std::list` allocate distinct heap nodes per element. Consequently, `push_back` and linear traversal (summing and `find`) exhibit closely matched throughput governed by CPU cache latency during pointer chasing.

2. **Sort Implementation Characteristics:**
   `Container::sort()` implements a recursive top-down merge sort (`merge_sort_chain`) operating by relinking node pointers without copies or temporary node allocations. In comparison, MSVC's `std::list::sort()` employs an iterative binning merge sort (array of 64 sublists) that reduces recursion stack frames and optimizes cache locality, outperforming `Container::sort()` at $n = 100\,000$ (8.44 ms vs. 11.42 ms). Both scale as expected with $O(n \log n)$ asymptotic complexity.

3. **Insertion Sort Complexity Verification:**
   On random data, `Container::insertion_sort()` clearly illustrates quadratic $O(n^2)$ behavior, escalating from 0.304 ms at $n = 1\,000$ to 88.228 ms at $n = 10\,000$ (a $\sim 290\times$ increase for a $10\times$ increase in $n$). On already-sorted data at $n = 100\,000$, insertion sort runs in just 0.367 ms, confirming its optimal linear $O(n)$ best-case complexity.

4. **Single-Element Removal vs. Bulk Erasure:**
   In the removal test, `titans::Container::remove_value` took 37.851 ms whereas `std::list::remove` took 91.625 ms. This divergence is explained by differing API semantics: `Container::remove_value` stops immediately upon unlinking the first matching node, requiring an average scan of $n / 2$ nodes per call. Conversely, `std::list::remove` must inspect every remaining node to remove all matching occurrences, resulting in a full traversal of the entire remaining list on every call.

# Performance & Benchmarks

This document outlines the performance characteristics of the C++ Limit Order Book (LOB) and Black-Scholes-Merton (BSM) Analytics Engine. All benchmarks are executed using [Google Benchmark](https://github.com/google/benchmark).

## Micro-Benchmarks (Google Benchmark)

The following metrics capture the nanosecond-level latency of core engine components. 

### Order Book (O(1) Operations)
The Orderbook utilizes a combination of `std::unordered_map` for $O(1)$ order tracking/cancellations and `std::map` (Red-Black Trees) for $O(\log N)$ price level insertions.

| Benchmark | Latency (ns) | CPU (ns) | Iterations | Description |
| :--- | :--- | :--- | :--- | :--- |
| `BM_Orderbook_AddCancel` | **~159 ns** | ~156 ns | 4,480,000 | Placing an order deep out-of-the-money and immediately canceling it. Tests the $O(1)$ hash map lookup and queue iterator erasure. |
| `BM_Orderbook_Match` | **~422 ns** | ~406 ns | 1,723,077 | Placing an aggressive market/limit order that crosses the spread and triggers a trade execution, removing liquidity from the opposing book. |

### Black-Scholes-Merton Engine (BSM)
The BSM engine is completely stateless, lock-free, and side-effect free. 

| Benchmark | Latency (ns) | CPU (ns) | Iterations | Description |
| :--- | :--- | :--- | :--- | :--- |
| `BM_BsmPricing` | **~1,013 ns** | ~977 ns | 640,000 | Calculates Option Price and all Greeks (Delta, Gamma, Vega, Theta, Rho) simultaneously using a fast CDF approximation. |
| `BM_BsmImpliedVolatility_ATM` | **~3,847 ns** | ~3,735 ns | 213,333 | Newton-Raphson numerical root-finding for an At-The-Money (ATM) option. Converges very rapidly due to high Vega. |
| `BM_BsmImpliedVolatility_DeepOTM`| **~72,009 ns** | ~71,150 ns | 11,200 | Worst-case scenario. Deep Out-Of-The-Money (OTM) options trigger the Zero-Vega trap, forcing the solver to gracefully fallback to a Bisection algorithm. |

---

## Stress Testing & Correctness

Beyond micro-benchmarks, the engine undergoes aggressive randomized stress testing to verify memory stability under load.

* **1 Million Operation Stress Test**: 
  The engine processes a deterministic seed of **1,000,000** randomized Order Additions, Cancellations, and Executions (GTC, FAK, Market Orders) in rapid succession.
  * **Result**: Zero memory leaks, zero segmentation faults. Total execution time under 25 seconds on a standard desktop CPU.
* **Invariant Validation**:
  During the 10,000-operation fuzz test, the book is halted after *every single operation* to assert internal structural invariants:
  1. The Bid stack is strictly descending (Best Bid at front).
  2. The Ask stack is strictly ascending (Best Ask at front).
  3. No crossed spreads exist in a resting state (Best Bid < Best Ask).
  4. Empty price levels are completely de-allocated.

## Reproducing Results

To compile and run the benchmark suite locally (Requires CMake 3.14+):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
./build/benchmark_engine.exe
```

*Note: Benchmarks were run on a local CPU environment. When deployed to a production high-frequency trading (HFT) server with custom memory pools (avoiding heap allocations) and lock-free concurrency, baseline latencies are expected to drop significantly.*

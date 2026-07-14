# C++ Limit Order Book (LOB) Matching Engine & Analytics Suite

A high-performance C++20 Limit Order Book (LOB) designed with modern systems programming and exchange microstructure standards in mind. The project is fully structured with a CMake build system and contains an extensive test suite built using Google Test, verifying correctness, performance under randomized stress scenarios, and advanced analytics capabilities.

---

## 🚀 Key Features

* **Price-Time Priority (FIFO)**: Matches bids and asks in accordance with standard exchange priority rules.
* **Order Types Supported**:
  * **Good-Till-Cancel (GTC)**: Standard resting limit orders.
  * **Fill-and-Kill (FAK)**: Immediate matching execution with automatic remainder cancellation (no resting portion).
  * **Market Orders**: Executes immediately against liquidity, sweeping multiple price levels if necessary.
* **Optimized Data Structures**:
  * $O(1)$ amortized order cancellation/modification by mapping Order IDs directly to queue iterators via a hash map (`std::unordered_map`).
  * $O(\log N)$ price level insertion using balanced binary search trees (`std::map`).
* **Advanced Trading Analytics & Pricing**:
  * **Order Book Snapshots**: Capture instantaneous book state and calculate Micro-Price to indicate true underlying asset value.
  * **VWAP Metrics**: Compute Volume-Weighted Average Price for executed trades and dynamic resting order book depth/volume.
  * **Order Book Imbalance (OBI)**: Calculate simple and depth-weighted order flow imbalances to gauge buy/sell pressure.
  * **Black-Scholes-Merton (BSM) Engine**: Integrated ultra-low-latency options pricing engine. Computes theoretical option prices and Greeks (Delta, Gamma, Vega, Theta, Rho) dynamically using orderbook micro-price as the spot price.
  * **Implied Volatility (IV) Solver**: Robust numerical root-finding solver (Newton-Raphson with guaranteed Bisection fallback) to extract market-implied volatility, protected against Zero-Vega traps and boundary violations.
  * **Dual-Dividend Modeling**: Supports Continuous Dividend Yield ($q$) for indices and FX options, and an Escrowed Dividend Model (handling exact Discrete Dividends) for single-name equities.
* **Automated Correctness Verification**:
  * Comprehensive custom Google Test cases validating engine logic, BSM outputs, and analytics.
  * Property-based invariant testing checking internal state validity after every single execution step.
  * 1,000,000 operation randomized stress test verifying stability and safety under extreme trade flows.

---

## 📁 Project Structure

```text
├── CMakeLists.txt              # Cross-platform build configuration
├── .gitignore                  # Professional git exclusions (build/ and binary outputs)
├── include/                    # Header files (Declarations)
│   ├── types.h                 # Quant price/qty typings and LevelInfo structures
│   ├── order.h                 # Order and OrderModify definitions
│   ├── trade.h                 # Trade execution structures
│   ├── orderbook.h             # Orderbook matching engine declaration
│   ├── book_snapshot.h         # Order book state snapshots and micro-price
│   ├── bsm_engine.h            # Stateless Black-Scholes-Merton pricing engine
│   ├── discrete_dividend.h     # Escrowed dividend model structures
│   └── option_pricer.h         # Options pricing using orderbook micro-prices
├── src/                        # Implementation files
│   ├── orderbook.cpp           # Main matching logic
│   ├── snapshot.cpp            # Book snapshot implementation
│   ├── imbalance.cpp           # Order Book Imbalance (OBI) calculations
│   ├── vwap.cpp                # VWAP and depth metrics
│   ├── bsm_engine.cpp          # Options pricing and Greeks calculations
│   ├── option_pricer.cpp       # Option pricer logic wrapping book snapshots
│   └── main.cpp                # Executable entry point
├── tests/                      # Testing directory
│   ├── test_order.cpp          # Unit tests for order state changes
│   ├── test_orderbook_basic.cpp# Basic FIFO, price priority, cancel/replace matching
│   ├── test_orderbook_types.cpp# Type-specific rules (Market and FAK executions)
│   ├── test_invariants.cpp     # Property-based testing & 1M-operation stress test
│   ├── test_bsm_engine.cpp     # Black-Scholes and Greeks math verification
│   └── test_option_pricer.cpp  # Dynamic option pricing logic verification
└── README.md                   
```

---

## 🛠️ Build & Run Instructions

This project uses **CMake** to automatically pull dependencies (Google Test) and compile the targets.

### Prerequisites
Make sure you have CMake (>= 3.14) and a C++ compiler supporting C++20 (GCC/MinGW, Clang, or MSVC) installed.

### 1. Configure the Build
Generate compiler-specific build files (this will automatically fetch and compile Google Test on the first run):
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

### 2. Compile the Targets
```bash
cmake --build build -j4
```

### 3. Run the Test Suite
Verify everything is working correctly with CTest:
```bash
ctest --test-dir build --output-on-failure
```

### 4. Run the Main Executable
```bash
./build/orderbook_main.exe
```

### 5. Run the Micro-Benchmarks
To run the Google Benchmark suite and measure the nanosecond latency of the BSM Engine and Orderbook:
```bash
./build/benchmark_engine.exe
```

---

## 🧪 Testing Strategy 

This project includes institutional-grade testing systems:

### 1. Property-Based Invariant Assertions
In `tests/test_invariants.cpp`, the book verifies structural invariants after every single trade, add, cancel, or modify action:
* **Sorted Bid Stack**: Best bid (highest price) is always at the front; bids are strictly sorted descending.
* **Sorted Ask Stack**: Best ask (lowest price) is always at the front; asks are strictly sorted ascending.
* **No Crossed Spread**: Best bid price is strictly less than the best ask price (otherwise a match should have occurred).
* **No Zero-Volume Levels**: Empty price levels are cleaned up immediately to prevent memory leaks and phantom matching.

### 2. Randomized Fuzz & Stress Testing
* **10,000 Action Loop**: Executes a deterministic seed sequence of random order insertions, cancels, and modifies, checking every single invariant rule after each step.
* **1,000,000 Action Stress Test**: Fires 1 million randomized orders (GTC, FAK, Market), cancels, and replaces to verify that the matching engine has zero segmentation faults, memory corruption, or performance bottlenecks under heavy volumes.

### 3. Analytics & Pricing Tests
* **BSM Engine Verification**: Validates the mathematical exactness of Delta, Gamma, Theta, Vega, Rho, and Option prices against known quantitative benchmarks.
* **Snapshot Integrations**: Ensures order book metrics (micro-price, imbalance) correctly update as limit orders are consumed.

> **Note**: For detailed nanosecond-level latency metrics and stress test results, see the [PERFORMANCE.md](PERFORMANCE.md) document.

---

## 🔮 Future Architecture (HFT Optimizations)
While the current implementation uses standard C++ STL containers (`std::map`, `std::shared_ptr`) to prioritize algorithmic clarity and testability, a production deployment to an ultra-low-latency environment would implement the following hardware-level optimizations:
1. **Custom Memory Pooling**: Replacing heap allocations (`std::make_shared`) with a pre-allocated contiguous `std::vector<Order>` pool. This eliminates latency spikes caused by OS-level memory allocation (`malloc`/`new`) during trading hours.
2. **Lock-Free Concurrency**: Splitting the architecture into a multi-threaded system where Market Data / Order ingestion runs on Thread A, and the heavy BSM / IV Solver runs on Thread B, communicating via an atomic Single-Producer Single-Consumer (SPSC) ring buffer to avoid mutex locking overhead.
3. **Array-Backed Price Levels**: Replacing the `std::map` Red-Black tree with a flat-array or sparse-hash structure to ensure CPU cache locality and eliminate pointer-chasing cache misses.

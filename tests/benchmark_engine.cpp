#include <benchmark/benchmark.h>
#include "bsm_engine.h"
#include "orderbook.h"
#include <random>

// ===================================================================
// BSM Engine Benchmarks (Phase 2 & 3)
// ===================================================================

static void BM_BsmPricing(benchmark::State& state) {
    // Standard ATM Option
    double S = 100.0;
    double K = 100.0;
    double T = 1.0;
    double r = 0.05;
    double sigma = 0.20;
    double q = 0.02;

    for (auto _ : state) {
        Greeks g = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma, q);
        benchmark::DoNotOptimize(g);
    }
}
BENCHMARK(BM_BsmPricing);

static void BM_BsmImpliedVolatility_ATM(benchmark::State& state) {
    // ATM options solve extremely fast (usually 3-4 iterations)
    double S = 100.0;
    double K = 100.0;
    double T = 1.0;
    double r = 0.05;
    double marketPrice = 10.45; // Approx 20% vol

    for (auto _ : state) {
        double iv = BsmEngine::SolveImpliedVolatility(OptionType::Call, marketPrice, S, K, T, r);
        benchmark::DoNotOptimize(iv);
    }
}
BENCHMARK(BM_BsmImpliedVolatility_ATM);

static void BM_BsmImpliedVolatility_DeepOTM(benchmark::State& state) {
    // Deep OTM triggers the Zero-Vega trap and forces Bisection (slower)
    double S = 100.0;
    double K = 150.0;
    double T = 1.0;
    double r = 0.05;
    double marketPrice = 0.10;

    for (auto _ : state) {
        double iv = BsmEngine::SolveImpliedVolatility(OptionType::Call, marketPrice, S, K, T, r);
        benchmark::DoNotOptimize(iv);
    }
}
BENCHMARK(BM_BsmImpliedVolatility_DeepOTM);

// ===================================================================
// Orderbook Matching Benchmarks (Phase 1)
// ===================================================================

static void BM_Orderbook_AddCancel(benchmark::State& state) {
    Orderbook book;
    OrderId id = 1;

    for (auto _ : state) {
        // Add a deep OTM resting order (won't match)
        auto order = std::make_shared<Order>(
            OrderType::GoodTillCancel, id++, Side::Buy, 90 * 100, 100
        );
        book.AddOrder(order);

        // Cancel it immediately (O(1) cancellation test)
        book.CancelOrder(order->GetOrderId());
    }
}
BENCHMARK(BM_Orderbook_AddCancel);

static void BM_Orderbook_Match(benchmark::State& state) {
    Orderbook book;
    OrderId id = 1;

    for (auto _ : state) {
        state.PauseTiming();
        // Setup: Place a resting ask
        auto ask = std::make_shared<Order>(
            OrderType::GoodTillCancel, id++, Side::Sell, 100 * 100, 100
        );
        book.AddOrder(ask);
        state.ResumeTiming();

        // Benchmark the matching: Place an aggressive buy that crosses the spread
        auto bid = std::make_shared<Order>(
            OrderType::GoodTillCancel, id++, Side::Buy, 101 * 100, 100
        );
        book.AddOrder(bid);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_Orderbook_Match);

#pragma once
#include "book_snapshot.h"
#include "bsm_engine.h"
#include "discrete_dividend.h"

// --- OptionParams ---
// Represents the external parameters needed to price an option.
// These parameters come from the contract specification and market data,
// NOT from the orderbook itself.
//
// Two dividend modes are supported:
//
// 1. Continuous dividend yield (q):
//    Used for INDEX options (S&P 500, NIFTY 50) and FX options.
//    Set q to the annualized dividend yield.
//
// 2. Discrete dividends:
//    Used for SINGLE-STOCK options (Apple, Reliance).
//    Leave q = 0.0.
//    Populate dividends with each upcoming ex-dividend event.
//    The pricer will compute their PV and subtract from the spot price.
struct OptionParams {
    OptionType type;  // Call or Put (from contract spec)

    // Strike price (from contract spec).
    double K;

    // Time to expiry (annualized).
    double T;

    // Risk-free interest rate.
    double r;

    // Implied volatility.
    double sigma;

    // Continuous dividend yield (for index/FX options).
    // Set to 0.0 when using discrete dividends.
    double q = 0.0;

    // Discrete dividends (for single-stock options).
    // Leave empty when using continuous yield q.
    DiscreteDividends dividends;
};

// --- OptionPricer ---
// A bridge that connects the Orderbook's top-of-book state (BookSnapshot)
// with the pure mathematical BSM engine.
//
// The pricer handles the dividend model:
// - If params.dividends is non-empty → Escrowed Dividend Model (adjust S)
// - Otherwise → pass continuous yield q to CalculateRisk
class OptionPricer {
public:
    // Prices an option using the micro-price from the provided snapshot as the
    // underlying asset's spot price (S).
    //
    // If the snapshot is invalid (e.g., one-sided book), returns zeroed Greeks.
    static Greeks PriceFromSnapshot(const BookSnapshot& snapshot, const OptionParams& params);

    // Solves for implied volatility using the micro-price from the snapshot.
    // Returns NaN if the market price is outside valid bounds.
    static double SolveIVFromSnapshot(const BookSnapshot& snapshot, const OptionParams& params,
                                       double marketPrice);
};

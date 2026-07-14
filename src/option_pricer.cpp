#include "option_pricer.h"
#include <cmath>

// --- Helper: Compute PV of discrete dividends ---
// Sums up the present value of all dividends that fall before option expiry.
//
// PV = Σ Di · e^(-r · ti)  for all ti < T
//
// WHY only dividends with ti < T?
// A dividend that occurs AFTER the option expires has no effect on the
// option's payoff. The stock will drop by the dividend amount, but our
// option holder won't be around to see it.
static double ComputeDividendPV(const DiscreteDividends& dividends, double r, double T) {
    double pv = 0.0;
    for (const auto& div : dividends) {
        // Only include dividends that fall before expiry
        if (div.time < T && div.time > 0.0) {
            pv += div.amount * std::exp(-r * div.time);
        }
    }
    return pv;
}

// --- Compute the effective spot price and dividend yield ---
struct AdjustedSpot {
    double S;
    double q;
};

static AdjustedSpot AdjustForDividends(double rawSpot, const OptionParams& params) {
    if (!params.dividends.empty()) {
        // Escrowed Dividend Model for single-stock options.
        // We subtract the present value of all upcoming dividends from the spot.
        // The BSM engine then sees a "clean" spot with q = 0.
        double pv = ComputeDividendPV(params.dividends, params.r, params.T);
        return { rawSpot - pv, 0.0 };
    }

    // Continuous yield for index/FX options.
    return { rawSpot, params.q };
}

Greeks OptionPricer::PriceFromSnapshot(const BookSnapshot& snapshot, const OptionParams& params) {
    // If the book is one-sided, we don't have a valid micro-price.
    if (!snapshot.valid) {
        return Greeks{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    }

    // Adjust spot for dividends (either discrete or continuous)
    auto [S, q] = AdjustForDividends(snapshot.microPrice, params);

    // Guard: if dividend PV exceeded the spot price, S is negative — no valid pricing
    if (S <= 0.0) {
        return Greeks{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    }

    return BsmEngine::CalculateRisk(params.type, S, params.K, params.T, params.r, params.sigma, q);
}

double OptionPricer::SolveIVFromSnapshot(const BookSnapshot& snapshot, const OptionParams& params,
                                          double marketPrice) {
    if (!snapshot.valid) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    auto [S, q] = AdjustForDividends(snapshot.microPrice, params);

    if (S <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return BsmEngine::SolveImpliedVolatility(params.type, marketPrice, S, params.K, params.T, params.r, q);
}

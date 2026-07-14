#pragma once
#include <cmath>

enum class OptionType { Call, Put };

struct Greeks {
    double price;
    double delta;
    double gamma;
    double vega;
    double theta;
    double rho;
};

// --- BsmEngine ---
// A completely stateless, deterministic mathematical engine for pricing Options.
// Does not allocate memory dynamically, ensuring ultra-low-latency execution.
//
// This engine implements the full Black-Scholes-Merton model with continuous
// dividend yield (q), making it suitable for:
//   - Index options (S&P 500, NIFTY 50) where q = annualized dividend yield
//   - FX options (Garman-Kohlhagen) where q = foreign risk-free rate
//   - Futures options (Black-76) where q = r (cost-of-carry cancels out)
//   - Single equities with no dividends where q = 0
//
// For single-stock discrete dividends, the caller (OptionPricer) adjusts the
// spot price BEFORE calling this engine. See discrete_dividend.h.
class BsmEngine {
public:
    // Calculates the Option Price and all first-order Greeks.
    //
    // S:     Spot Price (or dividend-adjusted spot for discrete dividends)
    // K:     Strike Price
    // T:     Time to Expiry (annualized fraction, e.g., 0.5 for 6 months)
    // r:     Risk-free Interest Rate (e.g., 0.05 for 5%)
    // sigma: Implied Volatility (e.g., 0.20 for 20%)
    // q:     Continuous dividend yield (default 0.0)
    //
    // The BSM formulas with dividends:
    //   d1 = [ln(S/K) + (r - q + σ²/2)T] / (σ√T)
    //   d2 = d1 - σ√T
    //   Call = S·e^(-qT)·N(d1) - K·e^(-rT)·N(d2)
    //   Put  = K·e^(-rT)·N(-d2) - S·e^(-qT)·N(-d1)
    static Greeks CalculateRisk(OptionType type, double S, double K, double T,
                                double r, double sigma, double q = 0.0);

    // --- Implied Volatility Solver ---
    //
    // Given an observed market price, finds the volatility (σ) that makes
    // BSM(σ) = marketPrice. BSM cannot be algebraically inverted for σ,
    // so numerical root-finding is used.
    //
    // Strategy:
    //   1. Newton-Raphson (primary): Uses Vega as the analytical derivative.
    //      Converges quadratically — usually 3-5 iterations for ATM options.
    //   2. Bisection (fallback): If Newton-Raphson diverges (Vega ≈ 0 for
    //      deep OTM/ITM options), the engine falls back to guaranteed-convergence
    //      bisection search within [0.0001, 5.0].
    //
    // Returns:
    //   The implied volatility σ* on success.
    //   NaN if no valid IV exists (price below intrinsic or above upper bound).
    //
    // Parameters:
    //   targetPrice: The observed market price of the option.
    //   S, K, T, r, q: Same as CalculateRisk.
    static double SolveImpliedVolatility(OptionType type, double targetPrice,
                                         double S, double K, double T,
                                         double r, double q = 0.0);

private:
    // Fast approximation of the Standard Normal Cumulative Distribution Function.
    // Uses Cody's rational Chebyshev approximation of erfc for near
    // full double precision (~15 significant digits) with minimal branching.
    static double FastNormalCDF(double x);
};

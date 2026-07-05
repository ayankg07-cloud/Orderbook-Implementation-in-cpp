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
class BsmEngine {
public:
    // Calculates the Option Price and all first-order Greeks.
    // S: Spot Price
    // K: Strike Price
    // T: Time to Expiry (annualized fraction, e.g., 0.5 for 6 months)
    // r: Risk-free Interest Rate (e.g., 0.05 for 5%)
    // sigma: Implied Volatility (e.g., 0.20 for 20%)
    static Greeks CalculateRisk(OptionType type, double S, double K, double T, double r, double sigma);

private:
    // Fast approximation of the Standard Normal Cumulative Distribution Function
    // Uses Cody's rational Chebyshev approximation of erfc for near
    // full double precision (~15 significant digits) with minimal branching.
    static double FastNormalCDF(double x);
};

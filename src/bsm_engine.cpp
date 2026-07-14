#include "bsm_engine.h"
#include <numbers>
#include <algorithm>
#include <limits>

// --- BsmEngine Math Helpers ---

// Cody's Rational Approximation of erfc
// "Rational Chebyshev Approximations for the Error Function"
//
// Computes Φ(x) = 0.5 * erfc(-x / √2) via three-region rational
// polynomial approximation, achieving near full double precision (~18 digits).
//
// Region 1: |arg| <= 0.46875  — erf via rational P/Q, erfc = 1 - erf
// Region 2: 0.46875 < |arg| <= 4.0 — erfc via rational P/Q
// Region 3: |arg| > 4.0 — erfc tail via rational P/Q with 1/x² scaling
double BsmEngine::FastNormalCDF(double x) {
    constexpr double INV_SQRT2 = 0.7071067811865475244;  // 1/√2
    constexpr double SQRPI     = 5.6418958354775628695e-1; // 1/√π
    constexpr double THRESH    = 0.46875;
    constexpr double XBIG      = 26.543;

    // --- Coefficients for erf approximation in Region 1 ---
    constexpr double A[] = {
        3.16112374387056560e+00,   // A[0] = A(1)
        1.13864154151050156e+02,   // A[1] = A(2)
        3.77485237685302021e+02,   // A[2] = A(3)
        3.20937758913846947e+03,   // A[3] = A(4)
        1.85777706184603153e-01    // A[4] = A(5)
    };
    constexpr double B[] = {
        2.36012909523441209e+01,   // B[0] = B(1)
        2.44024637934444173e+02,   // B[1] = B(2)
        1.28261652607737228e+03,   // B[2] = B(3)
        2.84423683343917062e+03    // B[3] = B(4)
    };

    // --- Coefficients for erfc approximation in Region 2 ---
    constexpr double C[] = {
        5.64188496988670089e-01,   // C[0] = C(1)
        8.88314979438837594e+00,   // C[1] = C(2)
        6.61191906371416295e+01,   // C[2] = C(3)
        2.98635138197400131e+02,   // C[3] = C(4)
        8.81952221241769090e+02,   // C[4] = C(5)
        1.71204761263407058e+03,   // C[5] = C(6)
        2.05107837782607147e+03,   // C[6] = C(7)
        1.23033935479799725e+03,   // C[7] = C(8)
        2.15311535474403846e-08    // C[8] = C(9)
    };
    constexpr double D[] = {
        1.57449261107098347e+01,   // D[0] = D(1)
        1.17693950891312499e+02,   // D[1] = D(2)
        5.37181101862009858e+02,   // D[2] = D(3)
        1.62138957456669019e+03,   // D[3] = D(4)
        3.29079923573345963e+03,   // D[4] = D(5)
        4.36261909014324716e+03,   // D[5] = D(6)
        3.43936767414372164e+03,   // D[6] = D(7)
        1.23033935480374942e+03    // D[7] = D(8)
    };

    // --- Coefficients for erfc approximation in Region 3 ---
    constexpr double P[] = {
        3.05326634961232344e-01,   // P[0] = P(1)
        3.60344899949804439e-01,   // P[1] = P(2)
        1.25781726111229246e-01,   // P[2] = P(3)
        1.60837851487422766e-02,   // P[3] = P(4)
        6.58749161529837803e-04,   // P[4] = P(5)
        1.63153871373020978e-02    // P[5] = P(6)
    };
    constexpr double Q[] = {
        2.56852019228982242e+00,   // Q[0] = Q(1)
        1.87295284992346047e+00,   // Q[1] = Q(2)
        5.27905102951428412e-01,   // Q[2] = Q(3)
        6.05183413124413191e-02,   // Q[3] = Q(4)
        2.33520497626869185e-03    // Q[4] = Q(5)
    };

    // Transform to erfc argument: Φ(x) = 0.5 * erfc(-x/√2)
    double arg = -x * INV_SQRT2;
    double y   = std::fabs(arg);
    double result;

    if (y <= THRESH) {
        // ---- Region 1: |arg| <= 0.46875, compute erf then erfc = 1 - erf ----
        double ysq = (y > 1.11e-16) ? y * y : 0.0;
        double xnum = A[4] * ysq;
        double xden = ysq;
        for (int i = 0; i < 3; ++i) {
            xnum = (xnum + A[i]) * ysq;
            xden = (xden + B[i]) * ysq;
        }
        // RESULT = X * (XNUM + A(4)) / (XDEN + B(4))
        double erf_val = arg * (xnum + A[3]) / (xden + B[3]);
        result = 1.0 - erf_val;  // erfc = 1 - erf for JINT=1

    } else if (y <= 4.0) {
        // ---- Region 2: 0.46875 < |arg| <= 4.0, erfc directly ----
        double xnum = C[8] * y;
        double xden = y;
        for (int i = 0; i < 7; ++i) {
            xnum = (xnum + C[i]) * y;
            xden = (xden + D[i]) * y;
        }
        result = (xnum + C[7]) / (xden + D[7]);

        // Stable exp(-y²) using AINT trick: exp(-ysq²) * exp(-del)
        double ysq = std::floor(y * 16.0) / 16.0;
        double del = (y - ysq) * (y + ysq);
        result = std::exp(-ysq * ysq) * std::exp(-del) * result;

        if (arg < 0.0) result = 2.0 - result;

    } else {
        // ---- Region 3: |arg| > 4.0, erfc tail ----
        if (y >= XBIG) {
            result = (arg >= 0.0) ? 0.0 : 2.0;
        } else {
            double ysq = 1.0 / (y * y);
            double xnum = P[5] * ysq;
            double xden = ysq;
            for (int i = 0; i < 4; ++i) {
                xnum = (xnum + P[i]) * ysq;
                xden = (xden + Q[i]) * ysq;
            }
            result = ysq * (xnum + P[4]) / (xden + Q[4]);
            result = (SQRPI - result) / y;

            // Stable exp(-y²)
            double ysq2 = std::floor(y * 16.0) / 16.0;
            double del = (y - ysq2) * (y + ysq2);
            result = std::exp(-ysq2 * ysq2) * std::exp(-del) * result;

            if (arg < 0.0) result = 2.0 - result;
        }
    }

    // Φ(x) = 0.5 * erfc(-x/√2)
    return 0.5 * result;
}

// --- BsmEngine Main Logic ---

// Full Black-Scholes-Merton with continuous dividend yield q.
//
// The dividend yield modifies the model in two fundamental ways:
// 1. The drift term in d1 becomes (r - q) instead of r.
//    WHY? A stock that pays dividends grows slower than one that doesn't,
//    because part of the return "leaks out" as dividend payments.
//
// 2. The spot price S is discounted by e^(-qT) in the pricing formula.
//    WHY? If one hold the stock for T years, one will receive dividends
//    worth approximately S * (1 - e^(-qT)). The option holder does NOT
//    receive these dividends (only the stock holder does), so the option
//    is priced on the "ex-dividend" forward value of the stock.
//
// When q = 0, this reduces to the standard Black-Scholes formula.

Greeks BsmEngine::CalculateRisk(OptionType type, double S, double K, double T,
                                double r, double sigma, double q) {
    Greeks greeks{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    if (T <= 0.0 || sigma <= 0.0 || S <= 0.0 || K <= 0.0) {
        if (T <= 0.0) {
            // At expiration payoff
            if (type == OptionType::Call) {
                greeks.price = std::max(0.0, S - K);
            } else {
                greeks.price = std::max(0.0, K - S);
            }
        }
        return greeks;
    }

    double sqrtT = std::sqrt(T);

    // d1 = [ln(S/K) + (r - q + σ²/2)T] / (σ√T)
    // Note: (r - q) replaces r in the standard formula.
    // When q = 0, this is identical to the original Black-Scholes d1.
    double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;

    double Nd1 = FastNormalCDF(d1);
    double Nd2 = FastNormalCDF(d2);

    // φ(d1) = (1/√2π) * e^(-d1²/2)  — Normal PDF for gamma/vega/theta
    constexpr double INV_SQRT_2PI = 0.3989422804014327;
    double Nd1_prime = INV_SQRT_2PI * std::exp(-0.5 * d1 * d1);

    double N_minus_d1 = FastNormalCDF(-d1);
    double N_minus_d2 = FastNormalCDF(-d2);

    double ert = std::exp(-r * T);  // Discount factor for strike
    double eqt = std::exp(-q * T);  // Discount factor for dividends

    // --- Pricing and Greeks ---
    // The key difference from basic BS: S is multiplied by e^(-qT) everywhere.
    // This "dividend discount" reduces the effective spot for the option holder,
    // since the option holder does not receive the dividends.

    if (type == OptionType::Call) {
        // Call = S·e^(-qT)·N(d1) - K·e^(-rT)·N(d2)
        greeks.price = S * eqt * Nd1 - K * ert * Nd2;

        // Delta(Call) = e^(-qT) · N(d1)
        // WHY e^(-qT)? Because delta measures ∂C/∂S, and the S term in the
        // pricing formula is S·e^(-qT), so the derivative picks up the e^(-qT).
        greeks.delta = eqt * Nd1;

        // Theta(Call) = -(S·e^(-qT)·φ(d1)·σ)/(2√T) + q·S·e^(-qT)·N(d1) - r·K·e^(-rT)·N(d2)
        // The +q·S·e^(-qT)·N(d1) term is new: it captures the value "leak"
        // from dividends as time passes. The option loses less value over time
        // because the underlying itself is losing value to dividends.
        greeks.theta = -(S * eqt * Nd1_prime * sigma) / (2.0 * sqrtT)
                       + q * S * eqt * Nd1
                       - r * K * ert * Nd2;

        // Rho(Call) = K·T·e^(-rT)·N(d2)
        // Rho is unchanged by dividends because it measures sensitivity
        // to the risk-free rate, not the dividend rate.
        greeks.rho = K * T * ert * Nd2;

    } else { // Put
        // Put = K·e^(-rT)·N(-d2) - S·e^(-qT)·N(-d1)
        greeks.price = K * ert * N_minus_d2 - S * eqt * N_minus_d1;

        // Delta(Put) = e^(-qT) · (N(d1) - 1) = -e^(-qT) · N(-d1)
        greeks.delta = eqt * (Nd1 - 1.0);

        // Theta(Put) = -(S·e^(-qT)·φ(d1)·σ)/(2√T) - q·S·e^(-qT)·N(-d1) + r·K·e^(-rT)·N(-d2)
        greeks.theta = -(S * eqt * Nd1_prime * sigma) / (2.0 * sqrtT)
                       - q * S * eqt * N_minus_d1
                       + r * K * ert * N_minus_d2;

        // Rho(Put) = -K·T·e^(-rT)·N(-d2)
        greeks.rho = -K * T * ert * N_minus_d2;
    }

    // Gamma and Vega are the same for Call and Put.
    // Both pick up the e^(-qT) factor from the S term.
    //
    // Gamma = e^(-qT) · φ(d1) / (S · σ · √T)
    greeks.gamma = eqt * Nd1_prime / (S * sigma * sqrtT);

    // Vega = S · e^(-qT) · √T · φ(d1)
    greeks.vega = S * eqt * sqrtT * Nd1_prime;

    return greeks;
}

// --- Implied Volatility Solver ---
//
// The objective function is:
//   f(σ) = BSM_Price(σ) - marketPrice = 0
//
// We want to find σ* such that f(σ*) = 0.
//
// Strategy:
//   Phase 1: Newton-Raphson (fast, quadratic convergence)
//     σ_{n+1} = σ_n - f(σ_n) / f'(σ_n)
//     where f'(σ_n) = Vega (the analytical derivative we already compute!)
//
//   Phase 2: Bisection fallback (guaranteed convergence)
//     If Newton-Raphson fails (Vega ≈ 0, divergence, out-of-bounds),
//     switch to bisection on [σ_low, σ_high] = [0.0001, 5.0].

double BsmEngine::SolveImpliedVolatility(OptionType type, double targetPrice,
                                          double S, double K, double T,
                                          double r, double q) {
    // --- Mathematical Constraints ---
    // These prevent the solver from searching for a root that doesn't exist.

    // Guard against degenerate inputs
    if (T <= 0.0 || S <= 0.0 || K <= 0.0 || targetPrice <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double ert = std::exp(-r * T);   // Discount factor for strike
    double eqt = std::exp(-q * T);   // Discount factor for dividends

    // Intrinsic Value Floor:
    // An option can never trade below its intrinsic value.
    // If the market price is below this floor, no real IV exists — it means the
    // market price is "impossible" under BSM (could be due to order book noise,
    // stale data, or illiquid markets).
    //
    // For a Call: price >= max(0, S·e^(-qT) - K·e^(-rT))
    // For a Put:  price >= max(0, K·e^(-rT) - S·e^(-qT))
    double intrinsic;
    if (type == OptionType::Call) {
        intrinsic = std::max(0.0, S * eqt - K * ert);
    } else {
        intrinsic = std::max(0.0, K * ert - S * eqt);
    }

    if (targetPrice < intrinsic - 1e-10) {
        // Price is below intrinsic value — no valid IV exists.
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Maximum Price Cap:
    // A call can never be worth more than the (dividend-discounted) stock itself.
    // A put can never be worth more than the (risk-free-discounted) strike.
    // If the market price exceeds this, the IV solver has no valid solution.
    double maxPrice;
    if (type == OptionType::Call) {
        maxPrice = S * eqt;
    } else {
        maxPrice = K * ert;
    }

    if (targetPrice > maxPrice + 1e-10) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // --- Solver Configuration ---
    constexpr int    MAX_NEWTON_ITER   = 10;     // Newton-Raphson iteration cap
    constexpr int    MAX_BISECTION_ITER = 100;    // Bisection iteration cap
    constexpr double TOLERANCE         = 1e-8;   // Price convergence tolerance
    constexpr double VEGA_FLOOR        = 1e-10;   // Zero-Vega trap threshold
    constexpr double SIGMA_LOW         = 1e-4;   // Bisection lower bound (0.01%)
    constexpr double SIGMA_HIGH        = 5.0;    // Bisection upper bound (500%)

    // --- Initial Guess ---
    // Brenner-Subrahmanyam heuristic: σ ≈ √(2π/T) · (Price/S)
    // WHY this formula? For at-the-money options (S ≈ K), the BSM call price
    // simplifies to approximately C ≈ S · σ · √(T/(2π)). Solving for σ gives
    // the formula below. It's a surprisingly good starting point.
    double sigma = std::sqrt(2.0 * std::numbers::pi / T) * (targetPrice / S);

    // Clamp the initial guess to a sane range
    sigma = std::clamp(sigma, SIGMA_LOW, SIGMA_HIGH);

    // --- Phase 1: Newton-Raphson ---
    bool newtonConverged = false;
    for (int i = 0; i < MAX_NEWTON_ITER; ++i) {
        Greeks g = CalculateRisk(type, S, K, T, r, sigma, q);

        double diff = g.price - targetPrice;

        // Check convergence: is the model price close enough to the market price?
        if (std::fabs(diff) < TOLERANCE) {
            newtonConverged = true;
            break;
        }

        // Zero-Vega Trap:
        // Deep out-of-the-money or deep in-the-money options have Vega ≈ 0.
        // If we divide by Vega ≈ 0, Newton's step σ_{n+1} = σ_n - diff/vega
        // will shoot off to ±infinity. We must catch this and bail to bisection.
        if (std::fabs(g.vega) < VEGA_FLOOR) {
            break;  // Fall through to bisection
        }

        // Newton-Raphson step: σ_{new} = σ - (BSM_Price - MarketPrice) / Vega
        sigma -= diff / g.vega;

        // If Newton shot σ out of bounds, bail to bisection
        if (sigma <= 0.0 || sigma > SIGMA_HIGH) {
            break;
        }
    }

    if (newtonConverged) {
        return sigma;
    }

    // --- Phase 2: Bisection Fallback ---
    // Newton-Raphson failed (diverged, Vega ≈ 0, or hit iteration cap).
    // Bisection is slower (linear convergence) but GUARANTEED to find the
    // root as long as f(σ_low) and f(σ_high) have opposite signs.
    //
    // The idea is simple: pick a low σ and a high σ. The BSM price at low σ
    // will be below the market price, and at high σ it will be above.
    // We keep cutting the interval in half, always picking the half where
    // the sign changes. After 100 iterations of halving, the interval
    // is 2^(-100) ≈ 10^(-30) wide — far beyond double precision.

    double lo = SIGMA_LOW;
    double hi = SIGMA_HIGH;

    // Verify that a root exists in [lo, hi] by checking sign change
    double fLo = CalculateRisk(type, S, K, T, r, lo, q).price - targetPrice;
    double fHi = CalculateRisk(type, S, K, T, r, hi, q).price - targetPrice;

    // If both endpoints have the same sign, no root exists in this interval
    if (fLo * fHi > 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    for (int i = 0; i < MAX_BISECTION_ITER; ++i) {
        double mid = 0.5 * (lo + hi);
        double fMid = CalculateRisk(type, S, K, T, r, mid, q).price - targetPrice;

        if (std::fabs(fMid) < TOLERANCE) {
            return mid;  // Converged
        }

        // Narrow the bracket: keep the half where the sign changes
        if (fLo * fMid < 0.0) {
            hi = mid;
            // fHi = fMid; 
        } else {
            lo = mid;
            fLo = fMid;
        }
    }

    // Return the midpoint of the final bracket as our best estimate
    return 0.5 * (lo + hi);
}

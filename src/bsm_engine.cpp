#include "bsm_engine.h"
#include <numbers>
#include <algorithm>

// Cody's Rational Approximation of erfc
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
        // Horner: XNUM = C(9)*Y; DO I=1,7: XNUM=(XNUM+C(I))*Y
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
            // erfc underflows to 0 for very large |arg|
            result = (arg >= 0.0) ? 0.0 : 2.0;
        } else {
            double ysq = 1.0 / (y * y);

            // Horner: XNUM = P(6)*YSQ; DO I=1,4: XNUM=(XNUM+P(I))*YSQ
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

Greeks BsmEngine::CalculateRisk(OptionType type, double S, double K, double T, double r, double sigma) {
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
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;

    double Nd1 = FastNormalCDF(d1);
    double Nd2 = FastNormalCDF(d2);

    // φ(d1) = (1/√2π) * e^(-d1²/2)  — inlined Normal PDF for gamma/vega/theta
    constexpr double INV_SQRT_2PI = 0.3989422804014327;
    double Nd1_prime = INV_SQRT_2PI * std::exp(-0.5 * d1 * d1);
    
    double N_minus_d1 = FastNormalCDF(-d1);
    double N_minus_d2 = FastNormalCDF(-d2);

    double ert = std::exp(-r * T);

    if (type == OptionType::Call) {
        greeks.price = S * Nd1 - K * ert * Nd2;
        greeks.delta = Nd1;
        greeks.theta = -(S * Nd1_prime * sigma) / (2.0 * sqrtT) - r * K * ert * Nd2;
        greeks.rho   = K * T * ert * Nd2;
    } else { 
        greeks.price = K * ert * N_minus_d2 - S * N_minus_d1;
        greeks.delta = Nd1 - 1.0;
        greeks.theta = -(S * Nd1_prime * sigma) / (2.0 * sqrtT) + r * K * ert * N_minus_d2;
        greeks.rho   = -K * T * ert * N_minus_d2;
    }

    // Gamma and Vega are the same for Call and Put
    greeks.gamma = Nd1_prime / (S * sigma * sqrtT);
    greeks.vega  = S * sqrtT * Nd1_prime;

    return greeks;
}

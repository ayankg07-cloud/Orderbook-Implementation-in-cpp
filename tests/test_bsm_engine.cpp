#include <gtest/gtest.h>
#include "bsm_engine.h"
#include <cmath>
#include <limits>

class BsmEngineTest : public ::testing::Test {
protected:

    const double S = 100.0;
    const double K = 100.0;
    const double T = 1.0;
    const double r = 0.05;
    const double sigma = 0.20;

    // Tolerance for fast approximations
    const double TOLERANCE = 1e-4;
};

// ===================================================================
// SECTION 1: Original BSM Tests (q = 0, backward compatible)
// ===================================================================

TEST_F(BsmEngineTest, AtTheMoneyCallPrice) {
    Greeks g = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma);
    EXPECT_NEAR(g.price, 10.45058, TOLERANCE);
}

TEST_F(BsmEngineTest, AtTheMoneyPutPrice) {
    Greeks g = BsmEngine::CalculateRisk(OptionType::Put, S, K, T, r, sigma);
    EXPECT_NEAR(g.price, 5.57352, TOLERANCE);
}

TEST_F(BsmEngineTest, CallPutParity) {
    Greeks callGreeks = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma);
    Greeks putGreeks = BsmEngine::CalculateRisk(OptionType::Put, S, K, T, r, sigma);

    // Call - Put = S - K * e^(-rT) (when q = 0)
    double callMinusPut = callGreeks.price - putGreeks.price;
    double expectedDiff = S - K * std::exp(-r * T);

    EXPECT_NEAR(callMinusPut, expectedDiff, TOLERANCE);
}

TEST_F(BsmEngineTest, CallDelta) {
    Greeks g = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma);
    EXPECT_NEAR(g.delta, 0.63683, TOLERANCE);
}

TEST_F(BsmEngineTest, PutDelta) {
    Greeks g = BsmEngine::CalculateRisk(OptionType::Put, S, K, T, r, sigma);
    EXPECT_NEAR(g.delta, -0.36317, TOLERANCE);
}

TEST_F(BsmEngineTest, GammaIsEqualForCallAndPut) {
    Greeks callGreeks = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma);
    Greeks putGreeks = BsmEngine::CalculateRisk(OptionType::Put, S, K, T, r, sigma);
    
    EXPECT_NEAR(callGreeks.gamma, putGreeks.gamma, 1e-9);
    EXPECT_NEAR(callGreeks.gamma, 0.01876, TOLERANCE);
}

TEST_F(BsmEngineTest, VegaIsEqualForCallAndPut) {
    Greeks callGreeks = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma);
    Greeks putGreeks = BsmEngine::CalculateRisk(OptionType::Put, S, K, T, r, sigma);
    
    EXPECT_NEAR(callGreeks.vega, putGreeks.vega, 1e-9);
    EXPECT_NEAR(callGreeks.vega, 37.524, 1e-2);
}

TEST_F(BsmEngineTest, ZeroTimeToExpire) {
    Greeks callGreeks = BsmEngine::CalculateRisk(OptionType::Call, 100.0, 90.0, 0.0, r, sigma);
    EXPECT_DOUBLE_EQ(callGreeks.price, 10.0);

    Greeks putGreeks = BsmEngine::CalculateRisk(OptionType::Put, 100.0, 110.0, 0.0, r, sigma);
    EXPECT_DOUBLE_EQ(putGreeks.price, 10.0);

    Greeks otmCall = BsmEngine::CalculateRisk(OptionType::Call, 100.0, 110.0, 0.0, r, sigma);
    EXPECT_DOUBLE_EQ(otmCall.price, 0.0);
}

// ===================================================================
// SECTION 2: Continuous Dividend Yield Tests (q != 0)
// ===================================================================

TEST_F(BsmEngineTest, DividendYieldReducesCallPrice) {
    // A stock paying dividends is worth less to a call holder (who doesn't
    // receive the dividends), so the call price should decrease.
    double q = 0.03; // 3% continuous dividend yield
    
    Greeks withoutDiv = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma, 0.0);
    Greeks withDiv    = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma, q);
    
    EXPECT_LT(withDiv.price, withoutDiv.price);
}

TEST_F(BsmEngineTest, DividendYieldIncreasesPutPrice) {
    // Conversely, dividends INCREASE put value because the stock drops more.
    double q = 0.03;
    
    Greeks withoutDiv = BsmEngine::CalculateRisk(OptionType::Put, S, K, T, r, sigma, 0.0);
    Greeks withDiv    = BsmEngine::CalculateRisk(OptionType::Put, S, K, T, r, sigma, q);
    
    EXPECT_GT(withDiv.price, withoutDiv.price);
}

TEST_F(BsmEngineTest, CallPutParityWithDividend) {
    // The generalized Put-Call Parity with dividends:
    //   C - P = S·e^(-qT) - K·e^(-rT)
    double q = 0.03;
    
    Greeks callGreeks = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma, q);
    Greeks putGreeks  = BsmEngine::CalculateRisk(OptionType::Put,  S, K, T, r, sigma, q);
    
    double callMinusPut = callGreeks.price - putGreeks.price;
    double expectedDiff = S * std::exp(-q * T) - K * std::exp(-r * T);
    
    EXPECT_NEAR(callMinusPut, expectedDiff, TOLERANCE);
}

TEST_F(BsmEngineTest, DividendDeltaIncludesDiscountFactor) {
    // Call Delta with dividends = e^(-qT) · N(d1)
    // This should be LESS than N(d1) (the q=0 delta)
    // Both the discount factor e^(-qT) < 1, and N(d1) decreases because d1 decreases.
    double q = 0.03;
    
    Greeks withDiv = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma, q);
    Greeks noDiv   = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma, 0.0);
    
    EXPECT_LT(withDiv.delta, noDiv.delta);
}

TEST_F(BsmEngineTest, FuturesOptionBlack76) {
    // For futures options, set q = r. This is the Black-76 model.
    // The forward price F = S·e^((r-q)T) = S when q = r.
    // Price should equal Black-76 result.
    double q = r;  // q = r for futures

    Greeks g = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma, q);

    // When q = r, Call = e^(-rT) * [S·N(d1) - K·N(d2)] with d1 using (r-q)=0
    // For ATM (S=K), d1 = σ√T / 2
    EXPECT_GT(g.price, 0.0);
    EXPECT_NEAR(g.delta, std::exp(-r * T) * 0.5, 0.05);
}

// ===================================================================
// SECTION 3: Implied Volatility Solver Tests
// ===================================================================

TEST_F(BsmEngineTest, IVSolver_ATM_Call) {
    // "Round-trip" test: compute a price with known sigma, then recover sigma.
    // This is the gold standard test: IV(Price(σ)) == σ
    double knownSigma = 0.20;
    Greeks g = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, knownSigma);
    
    double recoveredSigma = BsmEngine::SolveImpliedVolatility(
        OptionType::Call, g.price, S, K, T, r);
    
    EXPECT_NEAR(recoveredSigma, knownSigma, 1e-6);
}

TEST_F(BsmEngineTest, IVSolver_ATM_Put) {
    double knownSigma = 0.20;
    Greeks g = BsmEngine::CalculateRisk(OptionType::Put, S, K, T, r, knownSigma);
    
    double recoveredSigma = BsmEngine::SolveImpliedVolatility(
        OptionType::Put, g.price, S, K, T, r);
    
    EXPECT_NEAR(recoveredSigma, knownSigma, 1e-6);
}

TEST_F(BsmEngineTest, IVSolver_HighVolatility) {
    // Test with high vol (80%) — should still converge
    double knownSigma = 0.80;
    Greeks g = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, knownSigma);
    
    double recoveredSigma = BsmEngine::SolveImpliedVolatility(
        OptionType::Call, g.price, S, K, T, r);
    
    EXPECT_NEAR(recoveredSigma, knownSigma, 1e-5);
}

TEST_F(BsmEngineTest, IVSolver_LowVolatility) {
    // Test with low vol (5%) — should still converge
    double knownSigma = 0.05;
    Greeks g = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, knownSigma);
    
    double recoveredSigma = BsmEngine::SolveImpliedVolatility(
        OptionType::Call, g.price, S, K, T, r);
    
    EXPECT_NEAR(recoveredSigma, knownSigma, 1e-5);
}

TEST_F(BsmEngineTest, IVSolver_DeepITM_Call) {
    // Deep in-the-money call: S=100, K=60. Vega is very small here,
    // so Newton-Raphson may fail and we need the Bisection fallback.
    double knownSigma = 0.25;
    Greeks g = BsmEngine::CalculateRisk(OptionType::Call, 100.0, 60.0, T, r, knownSigma);
    
    double recoveredSigma = BsmEngine::SolveImpliedVolatility(
        OptionType::Call, g.price, 100.0, 60.0, T, r);
    
    EXPECT_NEAR(recoveredSigma, knownSigma, 1e-4);
}

TEST_F(BsmEngineTest, IVSolver_DeepOTM_Put) {
    // Deep out-of-the-money put: S=100, K=60. Very low Vega.
    double knownSigma = 0.30;
    Greeks g = BsmEngine::CalculateRisk(OptionType::Put, 100.0, 60.0, T, r, knownSigma);
    
    double recoveredSigma = BsmEngine::SolveImpliedVolatility(
        OptionType::Put, g.price, 100.0, 60.0, T, r);
    
    EXPECT_NEAR(recoveredSigma, knownSigma, 1e-4);
}

TEST_F(BsmEngineTest, IVSolver_WithDividendYield) {
    // Round-trip with continuous dividend yield
    double knownSigma = 0.25;
    double q = 0.02;
    
    Greeks g = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, knownSigma, q);
    
    double recoveredSigma = BsmEngine::SolveImpliedVolatility(
        OptionType::Call, g.price, S, K, T, r, q);
    
    EXPECT_NEAR(recoveredSigma, knownSigma, 1e-6);
}

TEST_F(BsmEngineTest, IVSolver_PriceBelowIntrinsic_ReturnsNaN) {
    // A call with S=100, K=90 has intrinsic value ~10 (PV-adjusted).
    // If the market price is $5 (below intrinsic), no valid IV exists.
    double iv = BsmEngine::SolveImpliedVolatility(
        OptionType::Call, 5.0, 100.0, 90.0, T, r);
    
    EXPECT_TRUE(std::isnan(iv));
}

TEST_F(BsmEngineTest, IVSolver_PriceAboveUpperBound_ReturnsNaN) {
    // A call can never be worth more than S. If market price = $110
    // and S = $100, no valid IV exists.
    double iv = BsmEngine::SolveImpliedVolatility(
        OptionType::Call, 110.0, 100.0, 100.0, T, r);
    
    EXPECT_TRUE(std::isnan(iv));
}

TEST_F(BsmEngineTest, IVSolver_NegativePrice_ReturnsNaN) {
    double iv = BsmEngine::SolveImpliedVolatility(
        OptionType::Call, -5.0, S, K, T, r);
    
    EXPECT_TRUE(std::isnan(iv));
}

TEST_F(BsmEngineTest, IVSolver_ShortExpiry) {
    // Very short time to expiry (1 day ≈ 1/252)
    double shortT = 1.0 / 252.0;
    double knownSigma = 0.30;
    
    Greeks g = BsmEngine::CalculateRisk(OptionType::Call, S, 99.0, shortT, r, knownSigma);
    
    double recoveredSigma = BsmEngine::SolveImpliedVolatility(
        OptionType::Call, g.price, S, 99.0, shortT, r);
    
    EXPECT_NEAR(recoveredSigma, knownSigma, 1e-3);
}

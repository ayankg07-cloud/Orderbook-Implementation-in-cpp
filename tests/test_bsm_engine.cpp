#include <gtest/gtest.h>
#include "bsm_engine.h"
#include <cmath>

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

    // Call - Put = S - K * e^(-rT)
    double callMinusPut = callGreeks.price - putGreeks.price;
    double expectedDiff = S - K * std::exp(-r * T);

    EXPECT_NEAR(callMinusPut, expectedDiff, TOLERANCE);
}

TEST_F(BsmEngineTest, CallDelta) {
    Greeks g = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma);
    // N(d1) where d1 = 0.35. N(0.35) is approx 0.6368
    EXPECT_NEAR(g.delta, 0.63683, TOLERANCE);
}

TEST_F(BsmEngineTest, PutDelta) {
    Greeks g = BsmEngine::CalculateRisk(OptionType::Put, S, K, T, r, sigma);
    // N(d1) - 1.0 = 0.63683 - 1.0 = -0.36317
    EXPECT_NEAR(g.delta, -0.36317, TOLERANCE);
}

TEST_F(BsmEngineTest, GammaIsEqualForCallAndPut) {
    Greeks callGreeks = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma);
    Greeks putGreeks = BsmEngine::CalculateRisk(OptionType::Put, S, K, T, r, sigma);
    
    // Gamma should be identical for Calls and Puts at same strike/expiry
    EXPECT_NEAR(callGreeks.gamma, putGreeks.gamma, 1e-9);
    // Known standard value: ~0.01876
    EXPECT_NEAR(callGreeks.gamma, 0.01876, TOLERANCE);
}

TEST_F(BsmEngineTest, VegaIsEqualForCallAndPut) {
    Greeks callGreeks = BsmEngine::CalculateRisk(OptionType::Call, S, K, T, r, sigma);
    Greeks putGreeks = BsmEngine::CalculateRisk(OptionType::Put, S, K, T, r, sigma);
    
    // Vega should be identical for Calls and Puts
    EXPECT_NEAR(callGreeks.vega, putGreeks.vega, 1e-9);
    // Known standard value: ~37.524
    EXPECT_NEAR(callGreeks.vega, 37.524, 1e-2);
}

TEST_F(BsmEngineTest, ZeroTimeToExpire) {
    // Exactly at expiration
    Greeks callGreeks = BsmEngine::CalculateRisk(OptionType::Call, 100.0, 90.0, 0.0, r, sigma);
    EXPECT_DOUBLE_EQ(callGreeks.price, 10.0); // Intrinsic value S - K

    Greeks putGreeks = BsmEngine::CalculateRisk(OptionType::Put, 100.0, 110.0, 0.0, r, sigma);
    EXPECT_DOUBLE_EQ(putGreeks.price, 10.0); // Intrinsic value K - S

    // OTM at expiration
    Greeks otmCall = BsmEngine::CalculateRisk(OptionType::Call, 100.0, 110.0, 0.0, r, sigma);
    EXPECT_DOUBLE_EQ(otmCall.price, 0.0);
}

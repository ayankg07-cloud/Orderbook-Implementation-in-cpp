#include <gtest/gtest.h>
#include "orderbook.h"
#include "option_pricer.h"

class OptionPricerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Standard option parameters for testing
        params.type = OptionType::Call;
        params.K = 100.0;
        params.T = 1.0;
        params.r = 0.05;
        params.sigma = 0.20;
    }

    Orderbook book;
    OptionParams params;
    const double TOLERANCE = 1e-4;
};

TEST_F(OptionPricerTest, SnapshotBasic) {
    // Add bids at $100.00
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 10000, 10));
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Buy, 10000, 20));
    
    // Add asks at $100.50
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 3, Side::Sell, 10050, 5));
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 4, Side::Sell, 10050, 15));

    BookSnapshot snapshot = book.GetSnapshot();

    EXPECT_TRUE(snapshot.valid);
    EXPECT_DOUBLE_EQ(snapshot.bestBid, 100.00);
    EXPECT_DOUBLE_EQ(snapshot.bestAsk, 100.50);
    EXPECT_DOUBLE_EQ(snapshot.bidVolume, 30.0);
    EXPECT_DOUBLE_EQ(snapshot.askVolume, 20.0);
    EXPECT_DOUBLE_EQ(snapshot.spread, 0.50);
    EXPECT_DOUBLE_EQ(snapshot.midPrice, 100.25);
}

TEST_F(OptionPricerTest, SnapshotEmptyBook) {
    BookSnapshot snapshot = book.GetSnapshot();
    EXPECT_FALSE(snapshot.valid);

    // One-sided book
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 10000, 10));
    snapshot = book.GetSnapshot();
    EXPECT_FALSE(snapshot.valid);
}

TEST_F(OptionPricerTest, SnapshotMicroPrice) {
    // Bid: $100 (100 lots)
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 10000, 100));
    // Ask: $101 (10 lots)
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Sell, 10100, 10));

    BookSnapshot snapshot = book.GetSnapshot();
    
    // microPrice = (bid * askVol + ask * bidVol) / (bidVol + askVol)
    // = (100.0 * 10.0 + 101.0 * 100.0) / 110.0
    // = (1000.0 + 10100.0) / 110.0 = 11100.0 / 110.0 = 100.909090...
    
    EXPECT_TRUE(snapshot.valid);
    EXPECT_NEAR(snapshot.microPrice, 100.90909, 1e-5);
    
    // Notice how micro-price is pulled heavily towards the ask ($101) 
    // because the bid size (100 lots) exerts buying pressure.
    EXPECT_DOUBLE_EQ(snapshot.midPrice, 100.50);
    EXPECT_GT(snapshot.microPrice, snapshot.midPrice);
}

TEST_F(OptionPricerTest, SnapshotConsistency) {
    // Bid: $100 (10 lots)
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 10000, 10));
    // Ask: $101 (10 lots)
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Sell, 10100, 10));

    // Take snapshot BEFORE modifying the book
    BookSnapshot snapshot1 = book.GetSnapshot();
    
    // Modify the book (simulate a torn read if we were reading directly)
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 3, Side::Buy, 10050, 50)); // New best bid

    // Compute greeks off the OLD snapshot
    Greeks greeks1 = OptionPricer::PriceFromSnapshot(snapshot1, params);

    // Compute greeks off a NEW snapshot
    BookSnapshot snapshot2 = book.GetSnapshot();
    Greeks greeks2 = OptionPricer::PriceFromSnapshot(snapshot2, params);

    // The greeks from snapshot1 should be consistent with the $100.50 mid/micro price,
    // totally unaffected by the new bid that arrived after the snapshot.
    // This proves the snapshot protects us from torn reads.
    EXPECT_DOUBLE_EQ(snapshot1.microPrice, 100.50);
    EXPECT_DOUBLE_EQ(snapshot2.microPrice, 100.91666666666667); // (100.5*10 + 101*50)/60
    
    EXPECT_NE(greeks1.price, greeks2.price);
}

TEST_F(OptionPricerTest, PriceFromSnapshot) {
    // S = 100.0 (balanced book with a spread so they don't match)
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 9950, 10));
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Sell, 10050, 10));

    BookSnapshot snapshot = book.GetSnapshot();
    Greeks greeks = OptionPricer::PriceFromSnapshot(snapshot, params);

    // Known standard value for ATM call (S=100, K=100, T=1, r=0.05, sigma=0.20)
    EXPECT_NEAR(greeks.price, 10.45058, TOLERANCE);
}

TEST_F(OptionPricerTest, MicroPriceVsMidPrice) {
    // Bid: $99 (100 lots)
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 9900, 100));
    // Ask: $101 (10 lots)
    book.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Sell, 10100, 10));
    
    BookSnapshot snapshot = book.GetSnapshot();
    
    // Mid price is $100
    EXPECT_DOUBLE_EQ(snapshot.midPrice, 100.0);
    
    // Micro price is pulled to $100.818... due to imbalance
    EXPECT_GT(snapshot.microPrice, 100.8);
    
    // Price with micro-price (S ~ 100.8)
    Greeks greeksMicro = OptionPricer::PriceFromSnapshot(snapshot, params);
    
    // Price with mid-price (S = 100.0) -> simulate by creating a balanced book
    Orderbook balancedBook;
    balancedBook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 9900, 10));
    balancedBook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Sell, 10100, 10));
    BookSnapshot balancedSnapshot = balancedBook.GetSnapshot();
    Greeks greeksMid = OptionPricer::PriceFromSnapshot(balancedSnapshot, params);
    
    // The call option should be more expensive when priced with micro-price
    // because the micro-price (S) is higher than the mid-price.
    EXPECT_GT(greeksMicro.price, greeksMid.price);
}

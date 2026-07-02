#include "orderbook.h"
#include <algorithm>
#include <numeric>

// --- Helper ---
static Quantity AggregateLevelQuantity(const OrderPointers& orders) {
    return std::accumulate(orders.begin(), orders.end(), (Quantity)0,
        [](Quantity runningSum, const OrderPointer& order) {
            return runningSum + order->GetRemainingQuantity();
        });
}

// --- GetTradedVWAP ---
double Orderbook::GetTradedVWAP(const Trades& trades) {
    if (trades.empty()) return 0.0;

    double totalValue = 0.0;
    std::uint64_t totalVolume = 0;

    for (const auto& trade : trades) {
        Quantity qty = trade.GetBidTrade().quantity_;
        Price price = trade.GetBidTrade().price_; 

        totalValue += static_cast<double>(price) * static_cast<double>(qty);
        totalVolume += qty;
    }

    if (totalVolume == 0) return 0.0;
    return totalValue / static_cast<double>(totalVolume);
}

// --- GetBookVWAPByDepth ---
double Orderbook::GetBookVWAPByDepth(Side side, std::size_t levels) const {
    if (levels == 0) return 0.0;

    double totalValue = 0.0;
    std::uint64_t totalVolume = 0;
    std::size_t count = 0;

    if (side == Side::Buy) {
        if (bids_.empty()) return 0.0;
        for (const auto& [price, orders] : bids_) {
            if (count >= levels) break;
            Quantity qty = AggregateLevelQuantity(orders);
            totalValue += static_cast<double>(price) * static_cast<double>(qty);
            totalVolume += qty;
            ++count;
        }
    } else {
        if (asks_.empty()) return 0.0;
        for (const auto& [price, orders] : asks_) {
            if (count >= levels) break;
            Quantity qty = AggregateLevelQuantity(orders);
            totalValue += static_cast<double>(price) * static_cast<double>(qty);
            totalVolume += qty;
            ++count;
        }
    }

    if (totalVolume == 0) return 0.0;
    return totalValue / static_cast<double>(totalVolume);
}

// --- GetBookVWAPByVolume ---
double Orderbook::GetBookVWAPByVolume(Side side, Quantity targetVolume) const {
    if (targetVolume == 0) return 0.0;

    double totalValue = 0.0;
    Quantity accumulatedVolume = 0;

    if (side == Side::Buy) {
        for (const auto& [price, orders] : bids_) {
            Quantity qty = AggregateLevelQuantity(orders);
            Quantity takeVolume = std::min(qty, targetVolume - accumulatedVolume);
            
            totalValue += static_cast<double>(price) * static_cast<double>(takeVolume);
            accumulatedVolume += takeVolume;
            
            if (accumulatedVolume >= targetVolume) break;
        }
    } else {
        for (const auto& [price, orders] : asks_) {
            Quantity qty = AggregateLevelQuantity(orders);
            Quantity takeVolume = std::min(qty, targetVolume - accumulatedVolume);
            
            totalValue += static_cast<double>(price) * static_cast<double>(takeVolume);
            accumulatedVolume += takeVolume;
            
            if (accumulatedVolume >= targetVolume) break;
        }
    }

    if (accumulatedVolume == 0) return 0.0;
    return totalValue / static_cast<double>(accumulatedVolume);
}

#include "orderbook.h"
#include <cmath>
#include <numeric>
// --- Helper ---
static Quantity AggregateQuantity(const OrderPointers& orders) {
    return std::accumulate(orders.begin(), orders.end(), (Quantity)0,
        [](Quantity runningSum, const OrderPointer& order) {
            return runningSum + order->GetRemainingQuantity();
        });
}
// --- GetSimpleObi ---
double Orderbook::GetSimpleObi() const {
    if(bids_.empty() || asks_.empty())
        return 0.0;

    std::uint64_t bidVolume = AggregateQuantity(bids_.begin()->second);
    std::uint64_t askVolume = AggregateQuantity(asks_.begin()->second);

    if(bidVolume + askVolume == 0)
        return 0.0;
    return static_cast<double>(static_cast<double>(bidVolume) - askVolume) / (bidVolume + askVolume);
}
// --- GetWeightedObi ---
double Orderbook::GetWeightedObi(std::size_t Levels) const {
    if(bids_.empty() || asks_.empty() || Levels == 0)
        return 0.0;
    
    double bestBid = static_cast<double>(bids_.begin()->first);
    double bestAsk = static_cast<double>(asks_.begin()->first);
    double midPrice = (bestBid + bestAsk) / 2.0;
    double totalWeightedBids = 0.0;
    double totalWeightedAsks = 0.0;
    
    std::size_t processedBidLevels = 0;
    for(const auto& [price, orders] : bids_)
    {
        if (processedBidLevels >= Levels) break;
        Quantity quantity = AggregateQuantity(orders);
        double distance = std::abs(static_cast<double>(price) - midPrice);
        if (distance > 0.0)
        {
            totalWeightedBids += static_cast<double>(quantity) / distance;
        }
        processedBidLevels++;
    }
    
    std::size_t processedAskLevels = 0;
    for(const auto& [price, orders] : asks_)
    {
        if (processedAskLevels >= Levels) break;
        Quantity quantity = AggregateQuantity(orders);
        double distance = std::abs(static_cast<double>(price) - midPrice);
        if (distance > 0.0)
        {
            totalWeightedAsks += static_cast<double>(quantity) / distance;
        }
        processedAskLevels++;
    }

    double totalWeight = totalWeightedBids + totalWeightedAsks;
    if (totalWeight == 0.0)
        return 0.0;
    
    return (totalWeightedBids - totalWeightedAsks) / totalWeight;
}

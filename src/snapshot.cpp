#include "orderbook.h"
#include <numeric>
#include <cmath> 
BookSnapshot Orderbook::GetSnapshot() const {
    BookSnapshot snapshot{};

    // If either bids or asks are empty, we can't compute a meaningful mid-price
    // or micro-price. We set valid = false and return early.
    if (bids_.empty() || asks_.empty()) {
        snapshot.valid = false;
        return snapshot;
    }

    snapshot.valid = true;
    // The orderbook uses integer cents (int32_t) to avoid floating-point errors
    // during matching. But pricing math (BSM) requires doubles.
    // This division (price / 100.0) is the "boundary" between the matching engine
    // and the pricing engine.
    const auto& [bestBidInt, bidOrders] = *bids_.begin();
    const auto& [bestAskInt, askOrders] = *asks_.begin();

    snapshot.bestBid = static_cast<double>(bestBidInt) / 100.0;
    snapshot.bestAsk = static_cast<double>(bestAskInt) / 100.0;

    // We only care about the best bid and best ask levels for micro-price.
    // We sum up the remaining quantity of all orders sitting at these levels.
    auto aggregateVolume = [](const OrderPointers& orders) {
        return std::accumulate(orders.begin(), orders.end(), (Quantity)0,
            [](Quantity sum, const OrderPointer& order) {
                return sum + order->GetRemainingQuantity();
            });
    };

    snapshot.bidVolume = static_cast<double>(aggregateVolume(bidOrders));
    snapshot.askVolume = static_cast<double>(aggregateVolume(askOrders));
    snapshot.spread = snapshot.bestAsk - snapshot.bestBid;
    snapshot.midPrice = (snapshot.bestBid + snapshot.bestAsk) / 2.0;

    double totalVolume = snapshot.bidVolume + snapshot.askVolume;

    if (totalVolume > 0.0) {
        snapshot.microPrice = (snapshot.bestBid * snapshot.askVolume + 
                               snapshot.bestAsk * snapshot.bidVolume) / totalVolume;
    } else {
        // Fallback to mid-price if volumes are somehow zero
        snapshot.microPrice = snapshot.midPrice;
    }

    return snapshot;
}

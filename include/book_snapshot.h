#pragma once
#include <cstdint>

// --- BookSnapshot ---
// A lightweight, immutable snapshot of the top-of-book state.
//
// WHY does this exist?
// If the engine read bestBid from bids_ and then bestAsk
// from asks_ at different moments during an AddOrder/CancelOrder/MatchOrders
// cycle, it might see a "half-applied" state.
// The micro-price computed from this torn state will jitter, causing
// greeks to spike tick-to-tick even when the true fair value hasn't moved.
// The fix: freeze all top-of-book data into this struct in ONE read pass,
// then compute everything (micro-price, greeks) off this frozen copy.
// This guarantees all derived quantities are internally consistent.
//
// WHY are the fields doubles, not int32_t like Price?
// The orderbook uses int32_t cents for exact price comparison (no float rounding).
// But BSM calls log(), exp(), sqrt() — these require double.
// The snapshot is the conversion boundary: int32_t cents → double dollars.
// This division happens ONCE per snapshot, not per greek computation.

struct BookSnapshot {
    double bestBid;      // Best bid price in dollars (converted from integer cents)
    double bestAsk;      // Best ask price in dollars (converted from integer cents)
    double bidVolume;    // Total quantity sitting at the best bid level
    double askVolume;    // Total quantity sitting at the best ask level
    double microPrice;   //better approximation than mid price
    double midPrice;     //used as a fallback if volumes are unavailable
    double spread;      
    bool valid;     
};

#pragma once
#include "book_snapshot.h"
#include "bsm_engine.h"
struct OptionParams {
    OptionType type;
    double K;         
    // Time to expiry (annualized).
    // Computed as (expiryTime - currentTime) / trading_days_per_year.
    double T;         
    
    // Risk-free interest rate (from external rates feed, e.g., SOFR).
    // Expressed as a decimal (e.g., 5.25% = 0.0525).
    double r;         
    
    // Implied volatility.
    double sigma;     
};

class OptionPricer {
public:
    // Prices an option using the micro-price from the provided snapshot as the
    // underlying asset's spot price (S).
    //
    // If the snapshot is invalid (e.g., one-sided book where micro-price cannot
    // be computed), it returns a zeroed-out Greeks struct.
    static Greeks PriceFromSnapshot(const BookSnapshot& snapshot, const OptionParams& params);
};

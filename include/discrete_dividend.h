#pragma once
#include <vector>

// --- DiscreteDividend ---
// Represents a single upcoming discrete dividend payment for a single-stock option.
//
// WHY do we need this?
// Index options (S&P 500, NIFTY) can use a continuous dividend yield (q)
// because hundreds of constituent stocks pay dividends on different days,
// creating a smooth "bleed" . But a single stock like Apple or Reliance pays
// a lump-sum dividend once a quarter or once a year.
//
// The fix: the Escrowed Dividend Model.Compute the Present Value (PV)
// of all dividends falling before the option's expiration:
//
//   PV = Σ Di · e^(-r · ti)
//
// Then subtract PV from the spot price and run standard BSM (with q=0):
//
//   S_adjusted = S - PV
//
// This correctly models the discrete price drop on the ex-dividend date.

struct DiscreteDividend {
    double amount;  // Dividend cash amount per share
    double time;    // Time to ex-dividend date in years
};

using DiscreteDividends = std::vector<DiscreteDividend>;

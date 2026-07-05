#include "option_pricer.h"
Greeks OptionPricer::PriceFromSnapshot(const BookSnapshot& snapshot, const OptionParams& params) {
    if (!snapshot.valid) {
        return Greeks{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    }

    // We use the micro-price as the spot price (S).
    // Since the snapshot already converted integer cents to double dollars,
    // we can pass it directly into the BSM engine.
    return BsmEngine::CalculateRisk(
        params.type,
        snapshot.microPrice, // S
        params.K,
        params.T,
        params.r,
        params.sigma
    );
}

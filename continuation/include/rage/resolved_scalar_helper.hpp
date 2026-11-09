#ifndef RAGE_RESOLVED_SCALAR_HELPER_HPP
#define RAGE_RESOLVED_SCALAR_HELPER_HPP

#include <optional>

namespace rage::offline {

struct ScalarHelperInputs {
    float target;
    float divisor;
    float lower_bound;
    float upper_bound;
    float tolerance;
    float requested_step;
    // Snapshot of [*(0x4252194c) + 0x24], needed for negative/unordered step.
    std::optional<float> fallback_step;
};

enum class ScalarHelperEffect {
    unchanged_equal_or_unordered,
    fallback_step_missing,
    target_stored,
    stepped_value_stored
};

// Bounded 41C784B0 value semantics, not its pointer-returning ABI.
// Requires binary32, round-to-nearest, no FTZ/DAZ or fast-math; excludes sNaNs
// and observable FP traps/flags. Missing fallback leaves value unchanged.
ScalarHelperEffect update_resolved_scalar(float& value,
                                          const ScalarHelperInputs& inputs);

} // namespace rage::offline
#endif

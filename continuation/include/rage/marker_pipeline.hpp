#ifndef RAGE_MARKER_PIPELINE_HPP
#define RAGE_MARKER_PIPELINE_HPP

#include "rage/position_lifecycle.hpp"

#include <optional>

namespace rage::offline {

struct MarkerPipelineInputs {
    bool derived_branch_taken;
    float marker_seed;
    std::uint8_t mode_byte;
    bool side54_seed_is_one;
    // Independent snapshots of [*(0x4252194c) + 0x24] at the two calls.
    std::optional<float> fallback_step50;
    std::optional<float> fallback_step54;
};

enum class MarkerPipelineOutcome {
    direct_seed_stored,
    derived_marker_stored,
    side50_fallback_missing,
    side54_fallback_missing
};

// Bounded helper/producer composition, with the scalar helper's FP assumptions.
// Inputs are snapshots, not aliases into N. Missing required observations leave
// N unchanged: an offline API convention, not an original rollback guarantee.
MarkerPipelineOutcome produce_marker_with_scalar_helpers(
    PositionRecord& record, MarkerPipelineInputs inputs);

} // namespace rage::offline
#endif

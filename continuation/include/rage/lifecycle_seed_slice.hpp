#ifndef RAGE_LIFECYCLE_SEED_SLICE_HPP
#define RAGE_LIFECYCLE_SEED_SLICE_HPP

#include "rage/marker_pipeline.hpp"

namespace rage::offline {

struct LifecycleSeedInputs {
    float now_at_845f;
    bool derived_branch_taken;
    std::uint8_t mode_byte;
    bool side54_seed_is_one;
    // Separate later observations, not aliases of now_at_845f or each other.
    std::optional<float> fallback_step50;
    std::optional<float> fallback_step54;
};

// Selected path 414F8459..414F84AE with AL != 0 at 414F8468, then the
// existing marker tail. N and prior guards are caller-resolved; earlier stores
// and other seed branches are excluded. Inherits the marker pipeline's FP and
// transactional missing-observation contracts, not original rollback semantics.
MarkerPipelineOutcome produce_marker_from_quarter_decay_seed(
    PositionRecord& record, LifecycleSeedInputs inputs);

} // namespace rage::offline
#endif

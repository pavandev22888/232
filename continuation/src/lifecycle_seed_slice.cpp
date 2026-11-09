#include "rage/lifecycle_seed_slice.hpp"

#include <cfloat>
#include <limits>

namespace rage::offline {

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(FLT_EVAL_METHOD == 0);

MarkerPipelineOutcome produce_marker_from_quarter_decay_seed(
    PositionRecord& record, LifecycleSeedInputs inputs)
{
    const float elapsed = inputs.now_at_845f - record.updated_at;
    const float scaled = elapsed * 0.25f; // 4206CDF4
    const float descending = 1.0f - scaled;
    const float lower_masked = scaled < 0.0f ? 1.0f : descending;
    const float upper_masked = 1.0f < scaled ? 0.0f : lower_masked;
    constexpr float cap = 0x1.99999ap-1f; // 42077FCC
    // MINSS selects the second operand on equality or unordered.
    const float seed = upper_masked < cap ? upper_masked : cap;
    return produce_marker_with_scalar_helpers(record,
        {inputs.derived_branch_taken, seed, inputs.mode_byte,
         inputs.side54_seed_is_one, inputs.fallback_step50,
         inputs.fallback_step54});
}

} // namespace rage::offline

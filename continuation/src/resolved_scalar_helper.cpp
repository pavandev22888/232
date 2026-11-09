#include "rage/resolved_scalar_helper.hpp"

#include <cfloat>
#include <cmath>
#include <limits>

namespace rage::offline {

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(FLT_EVAL_METHOD == 0);

ScalarHelperEffect update_resolved_scalar(float& value,
                                          const ScalarHelperInputs& inputs)
{
    // UCOMISS/JNE also returns without writing on unordered comparison.
    if (std::isnan(value) || std::isnan(inputs.target) || value == inputs.target)
        return ScalarHelperEffect::unchanged_equal_or_unordered;

    float step = inputs.requested_step;
    if (!(step >= 0.0f)) {
        if (!inputs.fallback_step)
            return ScalarHelperEffect::fallback_step_missing;
        step = *inputs.fallback_step;
    }

    const float delta = inputs.target - value;
    if (!(std::fabs(delta) > inputs.tolerance)) {
        value = inputs.target;
        return ScalarHelperEffect::target_stored;
    }

    const float scaled = step * delta;
    const float divided = scaled / inputs.divisor;
    const float candidate = divided + value;
    const bool choose_upper = inputs.upper_bound < candidate;
    // MAXSS selects its second operand on equality or unordered comparison.
    const float lower_selected = candidate > inputs.lower_bound
                                   ? candidate : inputs.lower_bound;
    value = choose_upper ? inputs.upper_bound : lower_selected;
    return ScalarHelperEffect::stepped_value_stored;
}

} // namespace rage::offline

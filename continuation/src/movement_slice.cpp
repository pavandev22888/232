#include "rage/movement_slice.hpp"

namespace rage::offline {

MovementCallerDecision evaluate_movement_caller_gate(
    const MovementCallerObservations& observations) noexcept
{
    if (!observations.selector_word.has_value()) {
        return {MovementCallerEdge::selector_unobserved, std::nullopt};
    }

    if (*observations.selector_word == 0x1fu ||
        *observations.selector_word == 0x40u) {
        return {MovementCallerEdge::selector_guard_to_4148d7b9,
                std::nullopt};
    }

    if (!observations.guard_byte.has_value()) {
        return {MovementCallerEdge::guard_unobserved, std::nullopt};
    }

    if ((*observations.guard_byte & 0x01u) != 0u) {
        return {MovementCallerEdge::predicate_skipped_to_4148cc8c,
                std::nullopt};
    }

    if (!observations.predicate_return_al.has_value()) {
        return {MovementCallerEdge::predicate_result_unknown,
                observations.predicate_argument};
    }

    if (*observations.predicate_return_al == 0u) {
        return {MovementCallerEdge::predicate_zero_to_4148d7b7,
                observations.predicate_argument};
    }

    return {MovementCallerEdge::predicate_nonzero_to_4148cc8c,
            observations.predicate_argument};
}

} // namespace rage::offline

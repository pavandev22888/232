#include "rage/movement_slice.hpp"

#include <cstring>
#include <emmintrin.h>
#include <limits>

namespace rage::offline {
namespace {
static_assert(sizeof(float) == sizeof(std::uint32_t));
static_assert(std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<float>::digits == 24);

std::uint32_t guard_sum(const MovementFs30Fields& fields) noexcept
{
    std::uint32_t sum = fields.field_a8;
    sum += fields.field_a4;
    sum += fields.field_b8;
    sum += fields.field_bc;
    sum += fields.field_64;
    return sum;
}

__m128 load_bits(Float32Bits bits) noexcept
{
    float value;
    std::memcpy(&value, &bits.value, sizeof(value));
    return _mm_set_ss(value);
}

Float32Bits store_bits(__m128 value) noexcept
{
    const float lane = _mm_cvtss_f32(value);
    Float32Bits bits{};
    std::memcpy(&bits.value, &lane, sizeof(lane));
    return bits;
}
} // namespace

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

MovementPredicateResult evaluate_movement_predicate(
    const MovementPredicateObservations& observations) noexcept
{
    MovementPredicateResult result{};
    if (!observations.first_guard.has_value())
        return result;

    result.first_guard_sum = guard_sum(*observations.first_guard);
    if (*result.first_guard_sum != 0x20u) {
        result.stop = MovementPredicateStop::alternate_control_414a518d;
        return result;
    }
    result.stop = MovementPredicateStop::slot_34_return_unobserved;
    if (!observations.slot_34_eax.has_value())
        return result;

    result.bl = 1u;
    if (*observations.slot_34_eax == 0u) {
        result.stop = MovementPredicateStop::second_guard_unobserved;
        if (!observations.second_guard.has_value())
            return result;

        result.second_guard_sum = guard_sum(*observations.second_guard);
        if (*result.second_guard_sum != 0x20u) {
            result.stop = MovementPredicateStop::alternate_control_414a523c;
            return result;
        }
        result.stop = MovementPredicateStop::slot_30_store_unobserved;
        if (!observations.slot_30_fstp32.has_value())
            return result;

        const __m128 square_root = _mm_sqrt_ss(
            load_bits(*observations.slot_30_fstp32));
        const __m128 scaled = _mm_mul_ss(square_root, _mm_set_ss(0.25f));
        const __m128 squared = _mm_mul_ss(scaled, scaled);
        result.floats = MovementPredicateFloatTrace{
            *observations.slot_30_fstp32, store_bits(square_root),
            store_bits(scaled), store_bits(squared), std::nullopt};

        result.stop = MovementPredicateStop::argument_unobserved;
        if (!observations.argument.has_value())
            return result;

        const __m128 masked_argument = _mm_and_ps(
            load_bits(*observations.argument),
            _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff)));
        result.floats->masked_argument = store_bits(masked_argument);
        // NOT ordered-greater preserves SETBE's true result for unordered inputs.
        result.bl = static_cast<std::uint8_t>(
            _mm_ucomigt_ss(masked_argument, squared) == 0);
    }

    result.stop = MovementPredicateStop::cookie_check_return_unobserved;
    if (!observations.cookie_check_returned_normally)
        return result;

    result.stop = MovementPredicateStop::returned_414a52db;
    result.return_al = result.bl;
    return result;
}

MovementCallerPredicateResult evaluate_movement_caller_and_predicate(
    std::optional<std::uint16_t> selector_word,
    std::optional<std::uint8_t> guard_byte,
    const MovementPredicateObservations& observations) noexcept
{
    MovementCallerObservations caller_observations{
        selector_word, guard_byte, observations.argument, std::nullopt};
    MovementCallerPredicateResult result{
        evaluate_movement_caller_gate(caller_observations), std::nullopt};
    if (result.caller.edge != MovementCallerEdge::predicate_result_unknown)
        return result;

    result.predicate = evaluate_movement_predicate(observations);
    caller_observations.predicate_return_al = result.predicate->return_al;
    result.caller = evaluate_movement_caller_gate(caller_observations);
    return result;
}

} // namespace rage::offline

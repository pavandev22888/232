#ifndef RAGE_MOVEMENT_SLICE_HPP
#define RAGE_MOVEMENT_SLICE_HPP

#include <cstdint>
#include <optional>

namespace rage::offline {

struct Float32Bits {
    std::uint32_t value;
};

enum class MovementCallerEdge : std::uint8_t {
    selector_unobserved,
    selector_guard_to_4148d7b9,
    guard_unobserved,
    predicate_skipped_to_4148cc8c,
    predicate_result_unknown,
    predicate_zero_to_4148d7b7,
    predicate_nonzero_to_4148cc8c,
};

struct MovementCallerObservations {
    std::optional<std::uint16_t> selector_word;
    std::optional<std::uint8_t> guard_byte;
    std::optional<Float32Bits> predicate_argument;
    std::optional<std::uint8_t> predicate_return_al;
};

struct MovementCallerDecision {
    MovementCallerEdge edge;
    std::optional<Float32Bits> forwarded_argument;
};

MovementCallerDecision evaluate_movement_caller_gate(
    const MovementCallerObservations& observations) noexcept;

struct MovementFs30Fields {
    std::uint32_t field_a8;
    std::uint32_t field_a4;
    std::uint32_t field_b8;
    std::uint32_t field_bc;
    std::uint32_t field_64;
};

struct MovementPredicateObservations {
    std::optional<MovementFs30Fields> first_guard;
    std::optional<std::uint32_t> slot_34_eax;
    std::optional<MovementFs30Fields> second_guard;
    // The binary32 store at 414A529F, not an invented x87 return conversion.
    std::optional<Float32Bits> slot_30_fstp32;
    std::optional<Float32Bits> argument;
    bool cookie_check_returned_normally = false;
};

enum class MovementPredicateStop : std::uint8_t {
    first_guard_unobserved,
    alternate_control_414a518d,
    slot_34_return_unobserved,
    second_guard_unobserved,
    alternate_control_414a523c,
    slot_30_store_unobserved,
    argument_unobserved,
    cookie_check_return_unobserved,
    returned_414a52db,
};

struct MovementPredicateFloatTrace {
    Float32Bits stored;
    Float32Bits square_root;
    Float32Bits scaled;
    Float32Bits squared;
    std::optional<Float32Bits> masked_argument;
};

struct MovementPredicateResult {
    MovementPredicateStop stop;
    std::optional<std::uint32_t> first_guard_sum;
    std::optional<std::uint32_t> second_guard_sum;
    std::optional<MovementPredicateFloatTrace> floats;
    std::optional<std::uint8_t> bl;
    std::optional<std::uint8_t> return_al;
};

// Site-specific observations; no calls to original code or replacement providers.
// Requires x86 SSE2, normal nested-call stack/EBX preservation, nearest-even,
// FTZ/DAZ off and masked FP exceptions. FP flags/traps are not an API output.
MovementPredicateResult evaluate_movement_predicate(
    const MovementPredicateObservations& observations) noexcept;

struct MovementCallerPredicateResult {
    MovementCallerDecision caller;
    std::optional<MovementPredicateResult> predicate;
};

// Reuses the W7 gate and forwards only AL established after the normal epilogue.
MovementCallerPredicateResult evaluate_movement_caller_and_predicate(
    std::optional<std::uint16_t> selector_word,
    std::optional<std::uint8_t> guard_byte,
    const MovementPredicateObservations& observations) noexcept;

} // namespace rage::offline

#endif

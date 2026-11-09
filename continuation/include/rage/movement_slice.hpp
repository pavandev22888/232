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

} // namespace rage::offline

#endif

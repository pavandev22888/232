#ifndef RAGE_ANGLE_SLICE_HPP
#define RAGE_ANGLE_SLICE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rage::offline {

struct AngleTailObservations {
    std::optional<std::uint16_t> call_414af106_ax;
    std::optional<std::uint16_t> call_414af11f_ax;
    std::optional<std::uint32_t> saved_word_after_second_call;
    std::optional<std::uint32_t> call_414af152_fstp32;
};

struct AngleTailDestination {
    std::uint8_t* data;
    std::size_t size;
};

enum class AngleTailStop {
    first_ax_unobserved,
    second_ax_unobserved,
    saved_word_unobserved,
    remainder_store_unobserved,
    invalid_destination_view,
    continued_414af6ad,
};

struct AngleWrapTrace {
    std::uint32_t subtract_360;
    bool above_180;
    std::uint32_t after_upper_selection;
    std::uint32_t add_360;
    bool below_minus_180;
    std::uint32_t wrapped;
};

struct AngleClampTrace {
    std::array<std::uint32_t, 2> packed_max;
    std::array<std::uint32_t, 2> packed_min;
    std::uint32_t scalar_max;
    std::uint32_t scalar_min;
};

struct AngleTailResult {
    AngleTailStop stop = AngleTailStop::first_ax_unobserved;
    std::optional<std::uint32_t> selected_input;
    std::optional<std::uint64_t> argument_binary64;
    std::optional<AngleWrapTrace> wrap;
    std::optional<AngleClampTrace> clamp;
    std::size_t stores_completed = 0;
};

// 414AF113..414AF1D6; the view is EDI memory at the later 414AF1A5 reload,
// not a pre-call command snapshot. Unknown calls supply independent observations.
// Requires GNU-compatible x86 SSE2, nearest-even, FTZ/DAZ off and masked FP exceptions.
// Flags/traps, call internals and whole-function reachability are not modeled.
AngleTailResult evaluate_angle_tail(
    const AngleTailObservations& observations,
    AngleTailDestination destination) noexcept;

} // namespace rage::offline

#endif

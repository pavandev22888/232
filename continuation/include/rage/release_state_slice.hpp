#ifndef RAGE_RELEASE_STATE_SLICE_HPP
#define RAGE_RELEASE_STATE_SLICE_HPP

#include <cstdint>
#include <optional>

namespace rage::offline {

enum class ReleaseLatchRoute { alternate_415e057c, dispatch_415e01d2, helper_414f2240 };

// Observations at 415E01A0/A8, after CALL 414F1240, not before it.
ReleaseLatchRoute release_latch_route(std::uint8_t mode_al,
                                      std::uint8_t pending_42500c78);

struct ReleaseLatchSnapshot {
    std::uint8_t byte_42506f30;
    std::uint32_t dword_4223fe28;
    std::uint8_t byte_42500c78;
};

// Entry 415E01B6: snapshot and source are observed AFTER CALL 414F2240.
ReleaseLatchSnapshot apply_release_after_helper(ReleaseLatchSnapshot snapshot,
                                               std::uint8_t helper_al,
                                               std::uint32_t current_424f4b18);

// Entry 415E01DF, AFTER the separate indirect call [425080E4].
// Missing field observation on equality is unresolved, not a skipped write.
std::optional<std::uint32_t> release_counter_after_dispatch(
    std::uint32_t counter_4223fe28, std::uint32_t current_424f4b18,
    std::optional<std::uint32_t> pointee_field_4d30);

} // namespace rage::offline
#endif

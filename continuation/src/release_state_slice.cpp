#include "rage/release_state_slice.hpp"

namespace rage::offline {

ReleaseLatchRoute release_latch_route(std::uint8_t mode_al,
                                      std::uint8_t pending_42500c78)
{
    if (mode_al == 0)
        return ReleaseLatchRoute::alternate_415e057c;
    return pending_42500c78 == 0 ? ReleaseLatchRoute::dispatch_415e01d2
                                : ReleaseLatchRoute::helper_414f2240;
}

ReleaseLatchSnapshot apply_release_after_helper(ReleaseLatchSnapshot snapshot,
                                               std::uint8_t helper_al,
                                               std::uint32_t current_424f4b18)
{
    if (helper_al == 0)
        snapshot.byte_42506f30 = 1;
    snapshot.dword_4223fe28 = current_424f4b18;
    snapshot.byte_42500c78 = 0;
    return snapshot;
}

std::optional<std::uint32_t> release_counter_after_dispatch(
    std::uint32_t counter_4223fe28, std::uint32_t current_424f4b18,
    std::optional<std::uint32_t> pointee_field_4d30)
{
    if (counter_4223fe28 != current_424f4b18)
        return counter_4223fe28;
    if (!pointee_field_4d30)
        return std::nullopt;
    return counter_4223fe28 + std::uint32_t{*pointee_field_4d30 != 0};
}

} // namespace rage::offline

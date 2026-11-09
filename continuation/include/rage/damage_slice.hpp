#ifndef RAGE_DAMAGE_SLICE_HPP
#define RAGE_DAMAGE_SLICE_HPP

#include <cstdint>
#include <optional>

namespace rage::offline {

// Models only the 41370710 hitgroup-3 arm; provider_value is its observed float return.
[[nodiscard]] std::optional<float> scale_hitgroup3_case(
    std::int32_t hitgroup,
    float provider_value) noexcept;

} // namespace rage::offline

#endif

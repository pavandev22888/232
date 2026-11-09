#include "rage/damage_slice.hpp"

#include <limits>

namespace rage::offline {
namespace {
static_assert(sizeof(float) == sizeof(std::uint32_t));
static_assert(std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<float>::digits == 24);

constexpr std::int32_t kHitgroup3 = 3;
constexpr float kHitgroup3Multiplier = 1.25f;
} // namespace

std::optional<float> scale_hitgroup3_case(
    std::int32_t hitgroup,
    float provider_value) noexcept
{
    if (hitgroup != kHitgroup3)
        return std::nullopt;

    float result = provider_value;
    result *= kHitgroup3Multiplier;
    return result;
}

} // namespace rage::offline

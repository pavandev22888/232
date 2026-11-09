#include "rage/bounded_aggregation.hpp"

#include <cfloat>
#include <limits>

namespace rage::offline {

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(FLT_EVAL_METHOD == 0);
static_assert(std::numeric_limits<std::uint32_t>::digits == 32);
static_assert(std::numeric_limits<std::int32_t>::digits == 31);

std::optional<TargetScoreProjection> accumulate_direct_score_prefix(
    const TargetScoreProjection& before, const ScorePointFields& point)
{
    if ((point.field_1c & 0xfffffffeu) != 2u)
        return std::nullopt;

    const std::uint32_t count = before.callback_count_bits + std::uint32_t{1};
    const float scaled_damage = point.damage_3c * 0.01f;
    const float weighted_point = scaled_damage * point.recorded_chance_34;
    const float sum = weighted_point + before.weighted_sum;
    return TargetScoreProjection{count, sum};
}

std::optional<std::int32_t> target_score_f0_write(
    const TargetScoreProjection& projection)
{
    const auto count_bits = projection.callback_count_bits;
    if (count_bits == 0 || (count_bits & 0x80000000u) != 0)
        return std::nullopt;

    const float count = static_cast<float>(count_bits);
    const float mean = projection.weighted_sum / count;
    // Masked CVTTSS2SI returns the integer-indefinite value on invalid input.
    if (!(mean >= -0x1p31f && mean < 0x1p31f))
        return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(mean);
}

} // namespace rage::offline

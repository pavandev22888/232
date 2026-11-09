#include "rage/bounded_aggregation.hpp"

#include <cfloat>
#include <limits>

namespace rage::offline {

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(FLT_EVAL_METHOD == 0);
static_assert(std::numeric_limits<std::uint32_t>::digits == 32);
static_assert(std::numeric_limits<std::int32_t>::digits == 31);

namespace {
float add_weighted_score(float before, const ScorePointFields& point)
{
    const float scaled_damage = point.damage_3c * 0.01f;
    const float weighted_point = scaled_damage * point.recorded_chance_34;
    return weighted_point + before;
}

bool signed_difference_greater_than_one(std::uint32_t left, std::uint32_t right)
{
    const std::uint32_t difference = left - right;
    return difference > 1u && difference < 0x80000000u;
}
} // namespace

std::optional<TargetScoreProjection> accumulate_direct_score_prefix(
    const TargetScoreProjection& before, const ScorePointFields& point)
{
    if ((point.field_1c & 0xfffffffeu) != 2u)
        return std::nullopt;

    const std::uint32_t count = before.callback_count_bits + std::uint32_t{1};
    const float sum = add_weighted_score(before.weighted_sum, point);
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

CallbackPrefixResult accumulate_callback_score_prefix(
    const TargetScoreProjection& before, const CallbackPointFields& point,
    const CallbackOwnerFields& owner, std::uint8_t accumulator_06,
    std::uint8_t accumulator_07,
    const std::optional<CallbackSettingsObservation>& settings)
{
    TargetScoreProjection after{before.callback_count_bits + std::uint32_t{1},
                                before.weighted_sum};
    const auto stop = [&](CallbackPrefixStop where) {
        return CallbackPrefixResult{after, where, std::nullopt};
    };
    const auto score = [&](std::uint8_t cl) {
        auto scored = after;
        scored.weighted_sum = add_weighted_score(after.weighted_sum, point.score);
        return CallbackPrefixResult{scored, CallbackPrefixStop::score_tail_4149e988, cl};
    };
    if ((point.score.field_1c & 0xfffffffeu) == 2u)
        return score(accumulator_07);
    if (owner.field_c0 != 0 && (owner.field_8c & 0xfffffffeu) != 2u)
        return score(accumulator_07);
    if (owner.field_c0 == 0 && accumulator_06 == 0)
        return score(accumulator_07);
    if (owner.field_c0 != 0 && accumulator_06 == 0 && accumulator_07 == 0)
        return score(0);

    if (!settings)
        return stop(CallbackPrefixStop::settings_index_unresolved);
    if (settings->index >= 16u)
        return stop(CallbackPrefixStop::unknown_index_handler_4149ec2d);
    if (!settings->selected_flags)
        return stop(CallbackPrefixStop::settings_flags_unresolved);
    const auto flags = *settings->selected_flags;
    const bool kind_one = point.score.field_1c == 1u;
    const bool difference_gate = kind_one && (flags & 8u) != 0 &&
        signed_difference_greater_than_one(owner.field_a0_bits, point.field_30_bits);

    if (owner.field_c0 != 0 && accumulator_06 == 0) {
        if (difference_gate)
            return score(0);
        if (kind_one && (flags & 4u) != 0 && point.field_25 != 0)
            return score(static_cast<std::uint8_t>(owner.field_138 != 0 && point.field_28 < 3));
        return score(1);
    }
    if ((owner.field_c0 != 0 && difference_gate) ||
        (owner.field_c0 == 0 && (flags & 8u) != 0))
        return score(accumulator_07);
    if (!kind_one || (flags & 4u) == 0 || point.field_25 == 0)
        return stop(CallbackPrefixStop::return_4149eb28);
    if (owner.field_138 == 0 || point.field_28 >= 3)
        return score(accumulator_07);
    return stop(CallbackPrefixStop::return_4149eb28);
}

CallbackTailStatistics update_callback_tail_statistics(
    CallbackTailStatistics before, const CallbackPointFields& point)
{
    if (point.score.recorded_chance_34 >= before.field_14) {
        before.field_14 = point.score.recorded_chance_34;
        // MAXSS selects its second operand on equality or unordered inputs.
        before.field_10 = before.field_10 > point.score.damage_3c
            ? before.field_10 : point.score.damage_3c;
    }
    if (point.field_25 != 0 && point.score.damage_3c > before.field_0c)
        before.field_0c = point.score.damage_3c;
    return before;
}

std::optional<CallbackInitialPoint> initialize_callback_point(
    std::uint8_t owner_c0, const std::array<std::uint8_t, 0x50>& point)
{
    if (owner_c0 != 0)
        return std::nullopt;
    CallbackInitialPoint result{};
    // Preserve the observed block order; inputs and destination do not alias.
    for (const std::size_t offset : {0x40u, 0x30u, 0x20u, 0x10u, 0x00u})
        for (std::size_t i = 0; i < 0x10; ++i)
            result.owner_70_bf[offset + i] = point[offset + i];
    result.owner_c0 = 1;
    return result;
}

} // namespace rage::offline

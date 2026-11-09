#include "rage/bounded_aggregation.hpp"

#include <cfloat>
#include <cstring>
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

std::optional<std::uint32_t> read_callback32(
    const std::vector<CallbackRaw32Region>& memory,
    std::uint32_t address, std::uint8_t width)
{
    if (std::uint64_t{address} + width > 0x100000000ull)
        return std::nullopt;
    std::uint32_t value = 0;
    for (std::uint32_t i = 0; i < width; ++i) {
        const std::uint32_t current = address + i;
        bool observed = false;
        for (const auto& region : memory) {
            if (current >= region.address &&
                std::uint64_t{current} - region.address < region.bytes.size()) {
                value |= std::uint32_t{region.bytes[current - region.address]} << (i * 8u);
                observed = true;
                break;
            }
        }
        if (!observed)
            return std::nullopt;
    }
    return value;
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

CallbackRankResult rank_callback_point(
    const CallbackInitialPoint& before,
    const std::array<std::uint8_t, 0x50>& point,
    std::uint8_t accumulator_05, std::uint8_t accumulator_08,
    std::uint8_t tail_cl, std::optional<std::uint8_t> owner_flags_b0)
{
    if (before.owner_c0 == 0)
        return {*initialize_callback_point(0, point), CallbackRankStop::return_4149eb28, true};

    const auto& saved = before.owner_70_bf;
    const auto word = [](const auto& bytes, std::size_t offset) {
        return std::uint32_t{bytes[offset]} | (std::uint32_t{bytes[offset + 1]} << 8) |
            (std::uint32_t{bytes[offset + 2]} << 16) | (std::uint32_t{bytes[offset + 3]} << 24);
    };
    const auto scalar = [&](const auto& bytes, std::size_t offset) {
        const auto raw = word(bytes, offset);
        float value;
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    };
    const auto signed_key = [](std::uint32_t value) { return value ^ 0x80000000u; };
    const auto finish = [&](bool replace) {
        CallbackRankResult result{before, CallbackRankStop::return_4149eb28, replace};
        if (replace) {
            // EAB9..EB28: six DWORDs, three 16-byte blocks, then one QWORD.
            const auto copy = [&](std::size_t offset, std::size_t count) {
                for (std::size_t i = 0; i < count; ++i)
                    result.after.owner_70_bf[offset + i] = point[offset + i];
            };
            for (std::size_t offset = 0; offset < 0x18; offset += 4)
                copy(offset, 4);
            for (const auto offset : {0x18u, 0x28u, 0x38u})
                copy(offset, 16);
            copy(0x48, 8);
        }
        return result;
    };
    const auto unresolved = [&] {
        return CallbackRankResult{before, CallbackRankStop::owner_flags_b0_unresolved, false};
    };
    const bool point_ge3 = signed_key(word(point, 0x28)) >= signed_key(3u);
    const bool saved_lt3 = signed_key(word(saved, 0x28)) < signed_key(3u);
    if (accumulator_08 != 0) {
        if (!owner_flags_b0)
            return unresolved();
        if ((*owner_flags_b0 & 4u) == 0) {
            if (point[0x25] != saved[0x25])
                return finish(point[0x25] != 0);
            if (point_ge3 == saved_lt3)
                return finish(point_ge3);
        } else {
            if (point_ge3 == saved_lt3)
                return finish(point_ge3);
            if (point[0x25] != saved[0x25])
                return finish(point[0x25] != 0);
        }
    }
    const bool point_direct = (word(point, 0x1c) & 0xfffffffeu) == 2u;
    const bool saved_direct = (word(saved, 0x1c) & 0xfffffffeu) == 2u;
    if (tail_cl != 0 && point_direct != saved_direct)
        return finish(point_direct);
    const bool point_pair = point[0x23] != 0 && point[0x25] != 0;
    const bool saved_pair = saved[0x23] != 0 && saved[0x25] != 0;
    if (point_pair != saved_pair)
        return finish(point_pair);

    const std::uint32_t difference = word(point, 0x30) - word(saved, 0x30);
    // SUB/NEG/CMOVS forms an unsigned magnitude, including 0x80000000.
    const std::uint32_t magnitude = (difference & 0x80000000u) != 0 ? 0u - difference : difference;
    const std::uint32_t limit = (point[0x23] != 0 && saved[0x23] != 0) || accumulator_05 != 0;
    const auto damage_wins = [&] { return scalar(point, 0x3c) > scalar(saved, 0x3c); };
    if (magnitude > limit)
        return finish(damage_wins());

    const float chance = scalar(point, 0x34);
    const float saved_chance = scalar(saved, 0x34);
    const float delta = chance - saved_chance;
    std::uint32_t absolute_bits;
    std::memcpy(&absolute_bits, &delta, sizeof(delta));
    absolute_bits &= 0x7fffffffu;
    float absolute_delta;
    std::memcpy(&absolute_delta, &absolute_bits, sizeof(absolute_delta));
    if (absolute_delta > 5.0f)
        return finish(chance > saved_chance);
    if (!owner_flags_b0)
        return unresolved();

    const std::size_t first = (*owner_flags_b0 & 4u) != 0 ? 0x28 : 0x2c;
    const std::size_t second = first == 0x28 ? 0x2c : 0x28;
    for (const auto offset : {first, second}) {
        const auto candidate = word(point, offset);
        const auto current = word(saved, offset);
        if (candidate != current)
            return finish(signed_key(candidate) > signed_key(current));
    }
    // UCOMISS/JNE falls through for both zero and unordered delta.
    if (delta < 0.0f || delta > 0.0f)
        return finish(chance > saved_chance);
    for (const auto offset : {0x21u, 0x22u})
        if (point[offset] != saved[offset])
            return finish(point[offset] != 0);
    if (scalar(saved, 0x38) >= scalar(point, 0x38))
        return finish(false);
    return finish(damage_wins());
}

std::array<std::uint8_t, 0x80> initialize_callback_context32(
    const std::array<std::uint8_t, 0x80>& before, std::uint32_t caller_address)
{
    auto after = before;
    const auto store = [&](std::size_t offset, std::uint32_t value) {
        for (std::size_t i = 0; i < 4; ++i)
            after[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
    };
    store(0x70, 0);
    store(0x30, 0);
    store(0x34, 0);
    store(0x38, 0);
    store(0x50, caller_address + 0x70u);
    store(0x54, caller_address + 0x80u);
    return after;
}

CallbackSettingsResolution resolve_callback_settings32(
    const std::vector<CallbackRaw32Region>& memory)
{
    using Stop = CallbackSettingsReadStop;
    CallbackSettingsResolution result{Stop::memory_unobserved, std::nullopt, std::nullopt};
    const auto read = [&](std::uint32_t address, std::uint8_t width) {
        const auto value = read_callback32(memory, address, width);
        if (!value)
            result.missing = CallbackMissingRead32{address, width};
        return value;
    };
    const auto root = read(0x424f639cu, 4);
    if (!root)
        return result;
    const auto manager = read(*root + 0x1012cu, 4);
    if (!manager)
        return result;
    const auto index = read(*manager + 0x2ccu, 4);
    if (!index)
        return result;
    result.observation = CallbackSettingsObservation{*index, std::nullopt};
    if (*index >= 16u) {
        result.stop = Stop::index_handler_4149ec2d;
        return result;
    }
    const std::uint32_t slot = *root + *index * 0x5d8u;
    const auto selector = read(slot + 0x1042cu, 1);
    if (!selector)
        return result;
    const auto flags = read(slot + *selector * 4u + 0x10424u, 4);
    if (!flags)
        return result;
    result.observation->selected_flags = *flags;
    result.stop = Stop::complete;
    return result;
}

CallbackOwnerFlagsResolution resolve_callback_owner_flags32(
    const std::vector<CallbackRaw32Region>& memory, std::uint32_t owner)
{
    const std::uint32_t pointer_address = owner + 4u;
    const auto pointer = read_callback32(memory, pointer_address, 4);
    if (!pointer)
        return {std::nullopt, CallbackMissingRead32{pointer_address, 4}};
    const std::uint32_t flags_address = *pointer + 0xb0u;
    const auto flags = read_callback32(memory, flags_address, 1);
    if (!flags)
        return {std::nullopt, CallbackMissingRead32{flags_address, 1}};
    return {static_cast<std::uint8_t>(*flags), std::nullopt};
}

} // namespace rage::offline

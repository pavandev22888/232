#include "rage/bounded_aggregation.hpp"

#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

using namespace rage::offline;

namespace {
int assertions = 0;
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; \
    std::exit(1); } } while (false)

float from_bits(std::uint32_t bits)
{
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t bits(float value)
{
    std::uint32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

TargetScoreProjection step(TargetScoreProjection before, ScorePointFields point)
{
    const auto after = accumulate_direct_score_prefix(before, point);
    CHECK(after.has_value());
    return *after;
}

void direct_prefix()
{
    CHECK(bits(0.01f) == 0x3c23d70au);
    const TargetScoreProjection before{7, 2.0f};
    for (const std::uint32_t kind : {2u, 3u}) {
        const auto after = step(before, {kind, 10.0f, 100.0f});
        CHECK(after.callback_count_bits == 8);
        CHECK(after.weighted_sum == 12.0f);
    }
    for (const std::uint32_t kind : {0u, 1u, 4u, 5u, 0x80000002u, 0xffffffffu})
        CHECK(!accumulate_direct_score_prefix(before, {kind, 10.0f, 100.0f}));
    CHECK(before.callback_count_bits == 7 && before.weighted_sum == 2.0f);

    const auto rounded = step({0, 0.0f}, {2, 90.0f, 10.0f});
    CHECK(bits(rounded.weighted_sum) == 0x410fffffu);
    const auto first = step({0, 0.0f}, {2, 10.0f, 100.0f});
    const auto second = step(first, {3, 90.0f, 10.0f});
    CHECK(second.callback_count_bits == 2);
    CHECK(second.weighted_sum == 19.0f);
    CHECK(target_score_f0_write(second) == 9);

    const auto wrapped = step({0xffffffffu, 1.0f}, {2, 1.0f, 100.0f});
    CHECK(wrapped.callback_count_bits == 0 && wrapped.weighted_sum == 2.0f);
    CHECK(!target_score_f0_write(wrapped));
    const auto signed_overflow = step({0x7fffffffu, 0.0f}, {2, 0.0f, 0.0f});
    CHECK(signed_overflow.callback_count_bits == 0x80000000u);
    CHECK(!target_score_f0_write(signed_overflow));
}

void floating_point_values()
{
    const auto negative_zero = step({0, -0.0f}, {2, 1.0f, -0.0f});
    CHECK(bits(negative_zero.weighted_sum) == 0x80000000u);
    CHECK(target_score_f0_write(negative_zero) == 0);
    const auto mixed_zero = step({0, 0.0f}, {2, 1.0f, -0.0f});
    CHECK(bits(mixed_zero.weighted_sum) == 0);
    const auto subnormal = step({0, from_bits(1)}, {2, 0.0f, 0.0f});
    CHECK(bits(subnormal.weighted_sum) == 1);
    CHECK(target_score_f0_write(subnormal) == 0);

    const float qnan = from_bits(0x7fc12345u);
    const float inf = std::numeric_limits<float>::infinity();
    for (const auto point : {ScorePointFields{2, qnan, 1.0f},
                             ScorePointFields{2, 1.0f, qnan},
                             ScorePointFields{2, 0.0f, inf}}) {
        const auto after = step({0, 0.0f}, point);
        CHECK(std::isnan(after.weighted_sum));
        CHECK(target_score_f0_write(after) == std::numeric_limits<std::int32_t>::min());
    }
    const auto nan_sum = step({0, qnan}, {3, 1.0f, 1.0f});
    CHECK(std::isnan(nan_sum.weighted_sum));
    const auto infinite = step({0, 0.0f}, {3, 1.0f, inf});
    CHECK(infinite.weighted_sum == inf);
    CHECK(target_score_f0_write(infinite) == std::numeric_limits<std::int32_t>::min());
}

void finalizer()
{
    const float qnan = from_bits(0x7fc12345u);
    for (const auto count : {0u, 0x80000000u, 0xffffffffu})
        CHECK(!target_score_f0_write({count, qnan}));
    CHECK(target_score_f0_write({2, 19.9f}) == 9);
    CHECK(target_score_f0_write({2, -19.9f}) == -9);
    CHECK(target_score_f0_write({1, 0x1.fffffep30f}) == 2147483520);
    CHECK(target_score_f0_write({1, -0x1p31f}) == std::numeric_limits<std::int32_t>::min());
    for (const auto value : {0x1p31f, -0x1.000002p31f, qnan,
                             std::numeric_limits<float>::infinity(),
                             -std::numeric_limits<float>::infinity()})
        CHECK(target_score_f0_write({1, value}) == std::numeric_limits<std::int32_t>::min());
    CHECK(target_score_f0_write({16777217u, 16777216.0f}) == 1);
    CHECK(target_score_f0_write({0x7fffffffu, 0x1p31f}) == 1);
}
void non_direct_prefix()
{
    using Stop = CallbackPrefixStop;
    const TargetScoreProjection before{0xffffffffu, 5.0f};
    CallbackPointFields point{{1u, 10.0f, 100.0f}, 1, 2, 10};
    CallbackOwnerFields owner{1, 2, 12, 1};
    const auto run = [&](std::uint8_t a6, std::uint8_t a7,
                         std::optional<CallbackSettingsObservation> settings) {
        return accumulate_callback_score_prefix(before, point, owner, a6, a7, settings);
    };
    const auto expect = [&](const CallbackPrefixResult& r, Stop stop,
                            std::optional<std::uint8_t> cl) {
        CHECK(r.after.callback_count_bits == 0);
        CHECK(r.stop == stop);
        CHECK(r.tail_cl == cl);
        CHECK(r.after.weighted_sum == (stop == Stop::score_tail_4149e988 ? 15.0f : 5.0f));
    };
    expect(run(1, 0x80, std::nullopt), Stop::settings_index_unresolved, std::nullopt);
    for (const auto index : {16u, 17u, 0x80000000u, 0xffffffffu})
        expect(run(1, 0x80, CallbackSettingsObservation{index, 12u}),
               Stop::unknown_index_handler_4149ec2d, std::nullopt);
    for (const auto index : {0u, 15u})
        expect(run(1, 0x80, CallbackSettingsObservation{index, std::nullopt}),
               Stop::settings_flags_unresolved, std::nullopt);

    for (const auto kind : {2u, 3u}) {
        point.score.field_1c = kind;
        expect(run(1, 0x80, std::nullopt), Stop::score_tail_4149e988, 0x80);
    }
    point.score.field_1c = 1;
    for (const auto kind : {0u, 1u, 4u, 0xffffffffu}) {
        owner.field_8c = kind;
        expect(run(1, 0xff, std::nullopt), Stop::score_tail_4149e988, 0xff);
    }
    owner.field_8c = 3;
    expect(run(0, 0, std::nullopt), Stop::score_tail_4149e988, 0);
    owner.field_c0 = 0;
    expect(run(0, 0x80, std::nullopt), Stop::score_tail_4149e988, 0x80);

    // Both C0 paths, including raw noncanonical booleans and signed CMP edges.
    for (const auto c0 : {0u, 1u, 0xffu}) {
        owner.field_c0 = static_cast<std::uint8_t>(c0);
        for (const auto flags : {0u, 4u, 8u, 12u, 0xffffffffu}) {
            for (const auto kind : {0u, 1u, 4u}) {
                point.score.field_1c = kind;
                for (const auto field25 : {0u, 1u, 0x80u}) {
                    point.field_25 = static_cast<std::uint8_t>(field25);
                    for (const auto field138 : {0u, 1u, 0x80u}) {
                        owner.field_138 = static_cast<std::uint8_t>(field138);
                        for (const auto n : {std::numeric_limits<std::int32_t>::min(),
                                             -1, 0, 2, 3, std::numeric_limits<std::int32_t>::max()}) {
                            point.field_28 = n;
                            const bool early_score = (flags & 8u) && (c0 == 0 || kind == 1);
                            const bool late_score = kind == 1 && (flags & 4u) && field25 &&
                                                    (!field138 || n >= 3);
                            const auto expected = early_score || late_score ? Stop::score_tail_4149e988
                                                                          : Stop::return_4149eb28;
                            expect(run(0xff, 0x80, CallbackSettingsObservation{15, flags}), expected,
                                   expected == Stop::score_tail_4149e988
                                       ? std::optional<std::uint8_t>{0x80} : std::nullopt);
                            if (c0 != 0) {
                                const std::uint8_t cl = (flags & 8u) && kind == 1 ? 0 :
                                    kind == 1 && (flags & 4u) && field25 ? (field138 && n < 3) : 1;
                                expect(run(0, 0x80, CallbackSettingsObservation{0, flags}),
                                       Stop::score_tail_4149e988, cl);
                            }
                        }
                    }
                }
            }
        }
    }
    owner = {1, 2, 0, 0};
    point = {{1, 10.0f, 100.0f}, 0, 0, 0};
    for (const auto difference : {0u, 1u, 2u, 0x7fffffffu, 0x80000000u, 0xffffffffu}) {
        point.field_30_bits = 0xfffffffeu;
        owner.field_a0_bits = point.field_30_bits + difference;
        const bool passes = difference >= 2u && difference <= 0x7fffffffu;
        expect(run(0, 1, CallbackSettingsObservation{0, 8u}), Stop::score_tail_4149e988,
               passes ? 0 : 1);
        expect(run(1, 1, CallbackSettingsObservation{0, 8u}),
               passes ? Stop::score_tail_4149e988 : Stop::return_4149eb28,
               passes ? std::optional<std::uint8_t>{1} : std::nullopt);
    }
    const auto wrapped_sign = accumulate_callback_score_prefix(
        {0x7fffffffu, 5}, point, owner, 1, 1, std::nullopt);
    CHECK(wrapped_sign.after.callback_count_bits == 0x80000000u);
}

void tail_statistics_and_initial_copy()
{
    CallbackPointFields point{{1, 20.0f, 7.0f}, 1, 0, 0};
    auto result = update_callback_tail_statistics({5, 10, 10}, point);
    CHECK(result.field_0c == 7 && result.field_10 == 10 && result.field_14 == 20);
    point.score.recorded_chance_34 = 9;
    point.score.damage_3c = 50;
    result = update_callback_tail_statistics({5, 10, 10}, point);
    CHECK(result.field_0c == 50 && result.field_10 == 10 && result.field_14 == 10);
    point.field_25 = 0;
    result = update_callback_tail_statistics({5, 10, 10}, point);
    CHECK(result.field_0c == 5);
    const float qnan = from_bits(0x7fc12345u);
    const float inf = std::numeric_limits<float>::infinity();
    for (const auto chance : {qnan, -inf}) {
        point.score = {1, chance, 50};
        result = update_callback_tail_statistics({5, 10, 10}, point);
        CHECK(result.field_10 == 10 && result.field_14 == 10);
    }
    point.score = {1, inf, 50};
    result = update_callback_tail_statistics({5, 10, qnan}, point);
    CHECK(result.field_10 == 10 && std::isnan(result.field_14));
    point.field_25 = 0x80;
    point.score = {1, 0.0f, -0.0f};
    result = update_callback_tail_statistics({0.0f, 0.0f, -0.0f}, point);
    CHECK(bits(result.field_0c) == 0);
    CHECK(bits(result.field_10) == 0x80000000u);
    CHECK(bits(result.field_14) == 0);
    point.score = {1, -0.0f, 0.0f};
    result = update_callback_tail_statistics({-0.0f, -0.0f, 0.0f}, point);
    CHECK(bits(result.field_0c) == 0x80000000u);
    CHECK(bits(result.field_10) == 0);
    CHECK(bits(result.field_14) == 0x80000000u);
    point.score = {1, 10, qnan};
    result = update_callback_tail_statistics({5, 10, 10}, point);
    CHECK(result.field_0c == 5 && std::isnan(result.field_10));
    point.score.damage_3c = 7;
    result = update_callback_tail_statistics({qnan, qnan, 10}, point);
    CHECK(std::isnan(result.field_0c) && result.field_10 == 7);
    point.score = {1, inf, inf};
    result = update_callback_tail_statistics({5, 10, 10}, point);
    CHECK(result.field_0c == inf && result.field_10 == inf && result.field_14 == inf);
    point.score = {1, from_bits(1), from_bits(2)};
    result = update_callback_tail_statistics({0, from_bits(1), 0}, point);
    CHECK(bits(result.field_0c) == 2 && bits(result.field_10) == 2 && bits(result.field_14) == 1);

    std::array<std::uint8_t, 0x50> raw{};
    for (std::size_t i = 0; i < raw.size(); ++i)
        raw[i] = static_cast<std::uint8_t>(i * 7);
    const auto copy = initialize_callback_point(0, raw);
    CHECK(copy && copy->owner_c0 == 1 && copy->owner_70_bf == raw);
    CHECK(!initialize_callback_point(1, raw));
    CHECK(!initialize_callback_point(0xff, raw));
}
} // namespace

int main()
{
    CHECK(std::fegetround() == FE_TONEAREST);
    direct_prefix();
    floating_point_values();
    finalizer();
    non_direct_prefix();
    tail_statistics_and_initial_copy();
    std::cout << assertions << " bounded aggregation assertions passed\n";
}

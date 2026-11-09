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
void ranking_tail()
{
    using Raw = std::array<std::uint8_t, 0x50>;
    const auto put = [](Raw& raw, std::size_t offset, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i)
            raw[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    };
    const auto fput = [&](Raw& raw, std::size_t offset, float value) { put(raw, offset, bits(value)); };
    const auto run = [&](const Raw& current, const Raw& point, std::uint8_t a5,
                         std::uint8_t a8, std::uint8_t cl, std::optional<std::uint8_t> flags,
                         CallbackRankStop stop, bool replace, std::uint8_t c0 = 0x80) {
        const CallbackInitialPoint before{current, c0};
        const auto result = rank_callback_point(before, point, a5, a8, cl, flags);
        CHECK(result.stop == stop);
        CHECK(result.replaced == replace);
        CHECK(result.after.owner_c0 == (c0 == 0 ? 1 : c0));
        CHECK(result.after.owner_70_bf == (replace ? point : current));
        CHECK(before.owner_70_bf == current && before.owner_c0 == c0);
    };
    const auto done = CallbackRankStop::return_4149eb28;
    const auto missing = CallbackRankStop::owner_flags_b0_unresolved;
    Raw saved{}, point{};
    for (std::size_t i = 0; i < point.size(); ++i)
        point[i] = static_cast<std::uint8_t>(i * 17 + 3);
    run(saved, point, 0, 0, 0, std::nullopt, done, true, 0);
    run(saved, point, 0, 1, 0, std::nullopt, missing, false);

    // Raw byte priority differs from boolean pair priority; flag 4 swaps priorities.
    for (const auto flags : {0u, 4u, 0xffu})
        for (const auto p25 : {0u, 1u, 2u, 0x80u, 0xffu})
            for (const auto s25 : {0u, 1u, 2u, 0x80u, 0xffu})
                for (const auto p28 : {0xffffffffu, 2u, 3u, 0x7fffffffu, 0x80000000u})
                    for (const auto s28 : {2u, 3u, 0x80000000u}) {
                        saved = {}; point = {};
                        saved[0x25] = s25; point[0x25] = p25;
                        put(saved, 0x28, s28); put(point, 0x28, p28);
                        put(point, 0x30, 2); fput(point, 0x3c, -1);
                        const bool pg = p28 >= 3 && p28 < 0x80000000u;
                        const bool sg = s28 >= 3 && s28 < 0x80000000u;
                        bool win = false;
                        if ((flags & 4u) != 0 && pg != sg) win = pg;
                        else if (p25 != s25) win = p25 != 0;
                        else if (pg != sg) win = pg;
                        run(saved, point, 0, 0xff, 0, static_cast<std::uint8_t>(flags), done, win);
                    }
    // The early kind/pair decisions need no B0 observation when +08 is zero.
    for (const auto pk : {0u, 1u, 2u, 3u, 0x80000002u})
        for (const auto sk : {0u, 2u, 3u})
            for (const auto cl : {0u, 1u, 0xffu}) {
                saved = {}; point = {};
                put(saved, 0x1c, sk); put(point, 0x1c, pk);
                put(point, 0x30, 2); fput(point, 0x3c, -1);
                const bool pd = pk == 2 || pk == 3, sd = sk == 2 || sk == 3;
                run(saved, point, 0, 0, cl, std::nullopt, done, cl != 0 && pd != sd && pd);
            }
    for (const auto p23 : {0u, 2u, 0xffu})
        for (const auto s23 : {0u, 0x80u})
            for (const auto p25 : {0u, 0x80u})
                for (const auto s25 : {0u, 0xffu}) {
                    saved = {}; point = {};
                    saved[0x23] = s23; point[0x23] = p23;
                    saved[0x25] = s25; point[0x25] = p25;
                    put(point, 0x30, 2); fput(point, 0x3c, -1);
                    run(saved, point, 0, 0, 0, std::nullopt, done,
                        (p23 != 0 && p25 != 0) && !(s23 != 0 && s25 != 0));
                }
    struct DifferenceCase { std::uint32_t p, s, magnitude; };
    for (const auto c : {DifferenceCase{0, 0, 0}, {1, 0, 1}, {0, 1, 1},
                         {0, 0xffffffffu, 1}, {0x80000000u, 0, 0x80000000u},
                         {0, 0x80000000u, 0x80000000u}, {0x7fffffffu, 0xffffffffu, 0x80000000u}})
        for (const auto a5 : {0u, 0x80u})
            for (const bool paired : {false, true}) {
                saved = {}; point = {};
                if (paired) saved[0x23] = point[0x23] = 0x80;
                put(saved, 0x30, c.s); put(point, 0x30, c.p);
                fput(point, 0x3c, 1);
                const bool wide = c.magnitude > static_cast<unsigned>(a5 != 0 || paired);
                run(saved, point, a5, 0, 0, std::nullopt, wide ? done : missing, wide);
            }
    // Near chance threshold, signed +28/+2C comparison precedes the finer chance test.
    for (const auto flags : {0u, 4u}) {
        saved = {}; point = {};
        put(point, 0x28, 2); put(saved, 0x28, 3);
        put(point, 0x2c, 1); put(saved, 0x2c, 0xffffffffu);
        fput(point, 0x34, 5.0f);
        run(saved, point, 0, 0, 0, flags, done, flags == 0);
        fput(point, 0x34, std::nextafter(5.0f, 6.0f));
        run(saved, point, 0, 0, 0, std::nullopt, done, true);
        fput(point, 0x34, -std::nextafter(5.0f, 6.0f));
        run(saved, point, 0, 0, 0, std::nullopt, done, false);
    }
    for (const auto offset : {0x28u, 0x2cu})
        for (const auto pv : {0u, 0x7fffffffu, 0x80000000u, 0xffffffffu})
            for (const auto sv : {0u, 0x7fffffffu, 0x80000000u, 0xffffffffu}) {
                saved = {}; point = {};
                put(point, offset, pv); put(saved, offset, sv);
                run(saved, point, 0, 0, 0, 0, done, (pv ^ 0x80000000u) > (sv ^ 0x80000000u));
            }
    const float qnan = from_bits(0x7fc01234u), inf = std::numeric_limits<float>::infinity();
    struct ChanceCase { float p, s; bool win; };
    for (const auto c : {ChanceCase{1, 0, true}, {-1, 0, false}, {from_bits(1), 0, true},
                         {-0.0f, 0.0f, false}, {inf, 0, true}, {0, inf, false},
                         {qnan, 0, false}, {0, qnan, false}, {inf, inf, false}}) {
        saved = {}; point = {};
        fput(point, 0x34, c.p); fput(saved, 0x34, c.s);
        run(saved, point, 0, 0, 0, 0, done, c.win);
    }
    // Unordered delta follows equality into byte tie-breaks (not C++ NaN != 0).
    for (const auto offset : {0x21u, 0x22u})
        for (const auto pv : {0u, 2u, 0x80u})
            for (const auto sv : {0u, 1u, 0xffu}) {
                saved = {}; point = {};
                point[offset] = pv; saved[offset] = sv;
                fput(point, 0x34, qnan);
                run(saved, point, 0, 0, 0, 0, done, pv != sv && pv != 0);
            }
    for (const auto p38 : {0.0f, 1.0f, -1.0f, qnan})
        for (const auto s38 : {0.0f, qnan})
            for (const auto damage : {1.0f, -1.0f, qnan, inf}) {
                saved = {}; point = {};
                fput(point, 0x38, p38); fput(saved, 0x38, s38);
                fput(point, 0x3c, damage);
                run(saved, point, 0, 0, 0, 4, done, !(s38 >= p38) && damage > 0);
            }
    // Replacement preserves every raw bit and the existing noncanonical C0 byte.
    saved = {};
    for (std::size_t i = 0; i < point.size(); ++i)
        point[i] = static_cast<std::uint8_t>(i * 23 + 11);
    point[0x25] = 0x80;
    put(point, 0x00, 0x7fa01234u); put(point, 0x04, 0x80000000u);
    run(saved, point, 0, 1, 0, 0, done, true, 0xff);
}
CallbackRaw32Region raw_word(std::uint32_t address, std::uint32_t word)
{
    return {address, {static_cast<std::uint8_t>(word),
                      static_cast<std::uint8_t>(word >> 8),
                      static_cast<std::uint8_t>(word >> 16),
                      static_cast<std::uint8_t>(word >> 24)}};
}

void callback_context_initialization()
{
    std::array<std::uint8_t, 0x80> before;
    for (std::size_t i = 0; i < before.size(); ++i)
        before[i] = static_cast<std::uint8_t>(0xa5u ^ i);
    for (const auto caller : {0u, 0x12345678u, 0xffffffe0u}) {
        const auto after = initialize_callback_context32(before, caller);
        auto expected = before;
        for (const auto offset : {0x70u, 0x30u, 0x34u, 0x38u, 0x50u, 0x54u}) {
            const std::uint32_t value = offset == 0x50u ? caller + 0x70u :
                offset == 0x54u ? caller + 0x80u : 0;
            for (std::size_t i = 0; i < 4; ++i)
                expected[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
        }
        for (std::size_t i = 0; i < before.size(); ++i)
            CHECK(after[i] == expected[i]);
    }
    const auto after = initialize_callback_context32(before, 0x1000u);
    const auto word = [&](std::size_t offset) {
        return std::uint32_t{after[offset]} | (std::uint32_t{after[offset + 1]} << 8) |
            (std::uint32_t{after[offset + 2]} << 16) | (std::uint32_t{after[offset + 3]} << 24);
    };
    CHECK(word(0x50) == 0x1070u && word(0x54) == 0x1080u);
    CHECK(word(0x70) == 0 && word(0x38) == 0);
    // The sum at S+98 is outside this projection and remains an independent observation.
    const auto prefix = accumulate_direct_score_prefix(
        {word(0x70), 7.0f}, {2, 10.0f, 100.0f});
    CHECK(prefix && prefix->callback_count_bits == 1 && prefix->weighted_sum == 17.0f);
    CHECK(word(0x38) == 0);
}

void raw32_dependencies()
{
    using Stop = CallbackSettingsReadStop;
    std::vector<CallbackRaw32Region> memory;
    const auto missing = [&](std::uint32_t address, std::uint8_t width,
                             std::optional<std::uint32_t> index) {
        const auto r = resolve_callback_settings32(memory);
        CHECK(r.stop == Stop::memory_unobserved);
        CHECK(r.missing && r.missing->address == address && r.missing->width == width);
        CHECK(r.observation.has_value() == index.has_value());
        if (index) {
            CHECK(r.observation->index == *index);
            CHECK(!r.observation->selected_flags);
        }
    };
    missing(0x424f639cu, 4, std::nullopt);
    memory.push_back({0x424f639cu, {0, 0x10, 0}});
    missing(0x424f639cu, 4, std::nullopt);
    memory[0] = raw_word(0x424f639cu, 0x1000u);
    missing(0x1112cu, 4, std::nullopt);
    memory.push_back(raw_word(0x1112cu, 0x2000u));
    missing(0x22ccu, 4, std::nullopt);
    memory.push_back(raw_word(0x22ccu, 15u));
    const std::uint32_t slot = 0x1000u + 15u * 0x5d8u;
    missing(slot + 0x1042cu, 1, 15u);
    memory.push_back({slot + 0x1042cu, {255}});
    missing(slot + 0x10424u + 255u * 4u, 4, 15u);
    memory.push_back(raw_word(slot + 0x10424u + 255u * 4u, 0x8000000cu));
    const auto resolved = resolve_callback_settings32(memory);
    CHECK(resolved.stop == Stop::complete && !resolved.missing);
    CHECK(resolved.observation->index == 15u);
    CHECK(resolved.observation->selected_flags == 0x8000000cu);
    const auto prefix = accumulate_callback_score_prefix(
        {0xffffffffu, -0.0f}, {{1u, 10.0f, 100.0f}, 1, 2, 0},
        {1, 2, 2, 1}, 1, 0xff, resolved.observation);
    CHECK(prefix.stop == CallbackPrefixStop::score_tail_4149e988);
    CHECK(prefix.after.callback_count_bits == 0 && prefix.after.weighted_sum == 10.0f);

    for (const auto index : {16u, 17u, 0x80000000u, 0xffffffffu}) {
        memory.resize(3);
        memory[2] = raw_word(0x22ccu, index);
        const auto r = resolve_callback_settings32(memory);
        CHECK(r.stop == Stop::index_handler_4149ec2d && !r.missing);
        CHECK(r.observation->index == index && !r.observation->selected_flags);
        const auto p = accumulate_callback_score_prefix(
            {1, -0.0f}, {{1u, 10.0f, 100.0f}, 1, 2, 0},
            {1, 2, 2, 1}, 1, 1, r.observation);
        CHECK(p.stop == CallbackPrefixStop::unknown_index_handler_4149ec2d);
        CHECK(p.after.callback_count_bits == 2 && bits(p.after.weighted_sum) == 0x80000000u);
    }

    // Selector 2 aliases the low byte of the selected DWORD; it is not clamped.
    for (const auto selector : {0u, 1u, 2u, 255u}) {
        const std::uint32_t root = 0xfffefbe0u;
        const std::uint32_t selector_address = root + 0x1042cu;
        const std::uint32_t flags_address = root + 0x10424u + selector * 4u;
        memory = {raw_word(0x424f639cu, root), raw_word(root + 0x1012cu, 0xfffffd34u),
                  raw_word(0, 0)};
        if (selector != 2u)
            memory.push_back({selector_address, {static_cast<std::uint8_t>(selector)}});
        memory.push_back(raw_word(flags_address, 0xaabbcc00u | selector));
        const auto r = resolve_callback_settings32(memory);
        CHECK(r.stop == Stop::complete && !r.missing);
        CHECK(r.observation->index == 0);
        CHECK(r.observation->selected_flags == (0xaabbcc00u | selector));
    }

    memory = {{0xfffffffeu, {0x50, 0xff}}, {0, {0xff, 0xff}}};
    const auto crossing = resolve_callback_owner_flags32(memory, 0xfffffffau);
    CHECK(!crossing.flags && crossing.missing->address == 0xfffffffeu &&
          crossing.missing->width == 4);
    memory.clear();
    auto flags = resolve_callback_owner_flags32(memory, 0xfffffffdu);
    CHECK(!flags.flags && flags.missing->address == 1 && flags.missing->width == 4);
    // A DWORD may span adjacent snapshot regions without host alignment requirements.
    memory = {{1, {0x50, 0xff}}, {3, {0xff, 0xff}}};
    flags = resolve_callback_owner_flags32(memory, 0xfffffffdu);
    CHECK(!flags.flags && flags.missing->address == 0 && flags.missing->width == 1);
    memory.push_back({0, {0xff}});
    flags = resolve_callback_owner_flags32(memory, 0xfffffffdu);
    CHECK(flags.flags == 0xff && !flags.missing);
    memory.back().bytes[0] = 0;
    flags = resolve_callback_owner_flags32(memory, 0xfffffffdu);
    CHECK(flags.flags == 0 && !flags.missing);

    CallbackInitialPoint saved{};
    saved.owner_c0 = 0x80;
    std::array<std::uint8_t, 0x50> point{};
    point[0x25] = 0x80;
    const auto rank = rank_callback_point(saved, point, 0, 1, 0, flags.flags);
    CHECK(rank.stop == CallbackRankStop::return_4149eb28 && rank.replaced);
    CHECK(rank.after.owner_c0 == 0x80 && rank.after.owner_70_bf == point);
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
    ranking_tail();
    callback_context_initialization();
    raw32_dependencies();
    std::cout << assertions << " bounded aggregation assertions passed\n";
}

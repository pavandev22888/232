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
} // namespace

int main()
{
    CHECK(std::fegetround() == FE_TONEAREST);
    direct_prefix();
    floating_point_values();
    finalizer();
    std::cout << assertions << " bounded aggregation assertions passed\n";
}

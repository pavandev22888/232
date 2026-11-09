#include "rage/resolved_scalar_helper.hpp"
#include "rage/position_lifecycle.hpp"

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

ScalarHelperInputs inputs(float target = 10.0f)
{
    return {target, 4.0f, -100.0f, 100.0f, 0.01f, 1.0f, std::nullopt};
}

void early_exits_and_fallback()
{
    const float nan = from_bits(0x7fc12345);
    auto args = inputs();
    args.requested_step = -1.0f;
    for (const auto pair : {std::pair<float, float>{-0.0f, 0.0f},
                            {nan, 1.0f}, {1.0f, nan},
                            {std::numeric_limits<float>::infinity(),
                             std::numeric_limits<float>::infinity()}}) {
        float value = pair.first;
        args.target = pair.second;
        CHECK(update_resolved_scalar(value, args) ==
              ScalarHelperEffect::unchanged_equal_or_unordered);
        CHECK(bits(value) == bits(pair.first));
    }

    args = inputs();
    args.requested_step = -1.0f;
    float value = 0.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::fallback_step_missing);
    CHECK(bits(value) == bits(0.0f));
    args.requested_step = nan;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::fallback_step_missing);
    args.fallback_step = 0.5f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(value == 1.25f);

    args = inputs();
    args.requested_step = -0.0f;
    value = 2.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(value == 2.0f);
    args.requested_step = -1.0f;
    args.tolerance = 100.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::fallback_step_missing);
    CHECK(value == 2.0f);
    args.fallback_step = -2.0f; // Fallback itself is not validated by the callee.
    args.tolerance = 0.01f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(value == -2.0f);
}

void threshold_arithmetic_and_selection()
{
    float value = 0.0f;
    auto args = inputs();
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(value == 2.5f);
    args = inputs(-10.0f);
    value = 0.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(value == -2.5f);

    args = inputs(2.0f);
    args.tolerance = 2.0f;
    args.lower_bound = 5.0f;
    args.upper_bound = 1.0f;
    value = 0.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::target_stored);
    CHECK(value == 2.0f); // Snap bypasses both bounds.
    args.tolerance = from_bits(0x7fc12345);
    value = 0.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::target_stored);
    CHECK(value == 2.0f);

    args = inputs();
    args.lower_bound = 3.0f;
    value = 0.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(value == 3.0f);
    args.upper_bound = 2.0f;
    value = 0.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(value == 2.0f);
    args.lower_bound = 10.0f;
    args.upper_bound = 3.0f;
    value = 0.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(value == 10.0f); // Upper comparison precedes lower selection.

    args = inputs(1.0f);
    args.requested_step = 0.0f;
    args.lower_bound = -0.0f;
    value = 0.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(bits(value) == bits(-0.0f));

    args = inputs();
    args.divisor = 0.0f;
    value = 0.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(value == 100.0f);
    args.requested_step = 0.0f; // 0/0 candidate: MAXSS chooses lower, not NaN.
    value = 0.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(value == -100.0f);

    args = inputs();
    args.lower_bound = from_bits(0x7fc12345);
    value = 0.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(bits(value) == 0x7fc12345);
    args = inputs();
    args.upper_bound = from_bits(0x7fc12345);
    value = 0.0f;
    CHECK(update_resolved_scalar(value, args) == ScalarHelperEffect::stepped_value_stored);
    CHECK(value == 2.5f);
}

void existing_n_observation_boundary()
{
    // 414F8753/877A: targets are caller seeds, current values are old N fields.
    float side50 = 0.0f;
    float side54 = 0.0f;
    const ScalarHelperInputs at50{1.0f, 0.25f, 0.0f, 1.0f, 0.01f, -1.0f, 0.125f};
    const ScalarHelperInputs at54{1.0f, 0.025f, 0.0f, 1.0f, 0.01f, -1.0f, 0.125f};
    CHECK(update_resolved_scalar(side50, at50) == ScalarHelperEffect::stepped_value_stored);
    CHECK(update_resolved_scalar(side54, at54) == ScalarHelperEffect::stepped_value_stored);
    CHECK(side50 == 0.5f);
    CHECK(side54 == 1.0f);

    PositionRecord record{};
    CHECK(produce_resolved_validity_marker(record,
          {true, 2.0f, 8, true, MarkerSideValues{side50, side54}}) ==
          MarkerProduction::derived_marker_stored);
    CHECK(record.validity_marker == 0.0f);
    float stored50;
    float stored54;
    std::memcpy(&stored50, reinterpret_cast<const unsigned char*>(&record) + 0x50, 4);
    std::memcpy(&stored54, reinterpret_cast<const unsigned char*>(&record) + 0x54, 4);
    CHECK(stored50 == side50);
    CHECK(stored54 == side54);

    const ScalarHelperInputs axis{10.0f, 0.085f,
        -std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
        0.01f, -1.0f, 0.0425f};
    float coordinate = 0.0f;
    CHECK(update_resolved_scalar(coordinate, axis) == ScalarHelperEffect::stepped_value_stored);
    CHECK(coordinate == 5.0f);
}
} // namespace

int main()
{
    early_exits_and_fallback();
    threshold_arithmetic_and_selection();
    existing_n_observation_boundary();
    std::cout << "PASS resolved_scalar_helper: " << assertions << " assertions\n";
}

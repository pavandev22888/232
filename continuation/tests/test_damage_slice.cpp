#include "rage/damage_slice.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

using rage::offline::scale_hitgroup3_case;

namespace {
int failures = 0;

void check(bool condition, const char* name)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

std::uint32_t bits(float value)
{
    std::uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(raw));
    return raw;
}
} // namespace

int main()
{
    const auto one = scale_hitgroup3_case(3, 1.0f);
    check(one.has_value(), "hitgroup 3 has a modeled result");
    check(one && bits(*one) == bits(1.25f), "multiplier is binary32 1.25");

    const auto fractional = scale_hitgroup3_case(3, 0.8f);
    check(fractional && bits(*fractional) == bits(1.0f),
          "provider float is multiplied before the float result is returned");

    const auto negative_zero = scale_hitgroup3_case(3, -0.0f);
    check(negative_zero && bits(*negative_zero) == bits(-0.0f),
          "positive multiplier preserves negative zero");

    const auto nan = scale_hitgroup3_case(
        3, std::numeric_limits<float>::quiet_NaN());
    check(nan && std::isnan(*nan), "NaN remains NaN through the local multiply");

    constexpr std::int32_t unmodeled[] = {0, 1, 2, 4, 8, -1};
    for (const auto other : unmodeled)
        check(!scale_hitgroup3_case(other, 4.0f),
              "unmodeled hitgroups have no invented fallback result");

    if (failures != 0)
        return 1;
    std::cout << "PASS: bounded hitgroup-3 float branch\n";
    return 0;
}

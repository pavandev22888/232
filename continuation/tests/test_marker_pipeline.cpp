#include "rage/marker_pipeline.hpp"

#include <array>
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

using Bytes = std::array<unsigned char, sizeof(PositionRecord)>;

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

Bytes snapshot(const PositionRecord& record)
{
    Bytes bytes;
    std::memcpy(bytes.data(), &record, bytes.size());
    return bytes;
}

void set_side(PositionRecord& record, std::size_t offset, float value)
{
    auto* bytes = reinterpret_cast<unsigned char*>(&record);
    std::memcpy(bytes + offset, &value, sizeof(value));
}

float side(const PositionRecord& record, std::size_t offset)
{
    float value;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&record);
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

PositionRecord fixture(float at50 = 0.25f, float at54 = 0.5f)
{
    PositionRecord record;
    Bytes bytes;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<unsigned char>((i * 37 + 11) & 0xff);
    std::memcpy(&record, bytes.data(), bytes.size());
    record.validity_marker = -9.0f;
    set_side(record, 0x50, at50);
    set_side(record, 0x54, at54);
    return record;
}

void check_untouched(const PositionRecord& record, const Bytes& before, bool derived)
{
    const auto after = snapshot(record);
    for (std::size_t i = 0; i < after.size(); ++i) {
        const bool changed_field = (i >= 0x48 && i < 0x4c) ||
                                   (derived && i >= 0x50 && i < 0x58);
        if (!changed_field)
            CHECK(after[i] == before[i]);
    }
}

void normal_and_target_selection()
{
    auto record = fixture();
    const auto before = snapshot(record);
    CHECK(produce_marker_with_scalar_helpers(record,
          {true, 1.0f, 8, false, 0.125f, 0.0125f}) ==
          MarkerPipelineOutcome::derived_marker_stored);
    CHECK(side(record, 0x50) == 0.625f);
    CHECK(side(record, 0x54) == 0.25f);
    CHECK(record.validity_marker == 0.515625f);
    check_untouched(record, before, true);

    for (std::uint8_t mode : {0, 1, 8, 255}) {
        for (bool target54 : {false, true}) {
            for (float seed : {1.0f, -2.0f, 0.5f,
                               std::numeric_limits<float>::infinity(),
                               -std::numeric_limits<float>::infinity()}) {
                record = fixture(0.5f, 0.5f);
                const auto saved = snapshot(record);
                auto expected = record;
                const float expected50 = mode == 8 ? 0.75f : 0.25f;
                const float expected54 = target54 ? 0.75f : 0.25f;
                CHECK(produce_resolved_validity_marker(expected,
                      {true, seed, mode, target54,
                       MarkerSideValues{expected50, expected54}}) ==
                      MarkerProduction::derived_marker_stored);
                CHECK(produce_marker_with_scalar_helpers(record,
                      {true, seed, mode, target54, 0.125f, 0.0125f}) ==
                      MarkerPipelineOutcome::derived_marker_stored);
                CHECK(bits(side(record, 0x50)) == bits(expected50));
                CHECK(bits(side(record, 0x54)) == bits(expected54));
                CHECK(snapshot(record) == snapshot(expected));
                check_untouched(record, saved, true);
            }
        }
    }
}

void bypasses_need_no_fallback()
{
    const float nan = from_bits(0x7fc12345);
    for (float seed : {0.0f, -0.0f, nan, from_bits(0xffc54321), 2.0f,
                       std::numeric_limits<float>::infinity()}) {
        for (bool derived : {false, true}) {
            if (derived && !std::isnan(seed) && seed != 0.0f)
                continue;
            for (std::uint8_t mode : {0, 8, 255}) {
                for (bool target54 : {false, true}) {
                    auto record = fixture(nan, -0.0f);
                    const auto before = snapshot(record);
                    auto expected = record;
                    const auto production = produce_resolved_validity_marker(expected,
                        {derived, seed, mode, target54, std::nullopt});
                    CHECK(production == (derived ? MarkerProduction::derived_marker_stored
                                                  : MarkerProduction::direct_seed_stored));
                    CHECK(produce_marker_with_scalar_helpers(record,
                          {derived, seed, mode, target54, std::nullopt, std::nullopt}) ==
                          (derived ? MarkerPipelineOutcome::derived_marker_stored
                                   : MarkerPipelineOutcome::direct_seed_stored));
                    CHECK(snapshot(record) == snapshot(expected));
                    if (!derived) {
                        CHECK(bits(record.validity_marker) == bits(seed));
                    } else {
                        CHECK(bits(side(record, 0x50)) == bits(mode == 8 ? 1.0f : 0.0f));
                        CHECK(bits(side(record, 0x54)) == bits(target54 ? 1.0f : 0.0f));
                        CHECK(std::isnan(seed) ? std::isnan(record.validity_marker)
                                              : bits(record.validity_marker) == bits(seed));
                    }
                    check_untouched(record, before, derived);
                }
            }
        }
    }
}

void equality_unordered_and_signed_zero()
{
    const float nan50 = from_bits(0x7fc12345);
    const float nan54 = from_bits(0xffc54321);
    for (const auto& values : {MarkerSideValues{1.0f, 1.0f},
                               {nan50, 1.0f}, {1.0f, nan54}, {nan50, nan54}}) {
        auto record = fixture(values.at_50, values.at_54);
        const auto before = snapshot(record);
        CHECK(produce_marker_with_scalar_helpers(record,
              {true, 2.0f, 8, true, std::nullopt, std::nullopt}) ==
              MarkerPipelineOutcome::derived_marker_stored);
        CHECK(bits(side(record, 0x50)) == bits(values.at_50));
        CHECK(bits(side(record, 0x54)) == bits(values.at_54));
        CHECK((std::isnan(values.at_50) || std::isnan(values.at_54))
                  ? std::isnan(record.validity_marker) : record.validity_marker == 0.0f);
        check_untouched(record, before, true);
    }
    for (float zero50 : {0.0f, -0.0f}) {
        for (float zero54 : {0.0f, -0.0f}) {
            auto record = fixture(zero50, zero54);
            CHECK(produce_marker_with_scalar_helpers(record,
                  {true, -2.0f, 0, false, std::nullopt, std::nullopt}) ==
                  MarkerPipelineOutcome::derived_marker_stored);
            CHECK(bits(side(record, 0x50)) == bits(zero50));
            CHECK(bits(side(record, 0x54)) == bits(zero54));
            CHECK(record.validity_marker == -2.0f);
        }
    }
}

void unresolved_is_transactional_and_ordered()
{
    for (const auto& values : {MarkerSideValues{0.25f, 0.5f},
                               {0.005f, 0.5f}}) {
        auto record = fixture(values.at_50, values.at_54);
        const auto before = snapshot(record);
        CHECK(produce_marker_with_scalar_helpers(record,
              {true, 1.0f, 0, false, std::nullopt, std::nullopt}) ==
              MarkerPipelineOutcome::side50_fallback_missing);
        CHECK(snapshot(record) == before);
    }
    for (const auto& values : {MarkerSideValues{0.25f, 0.5f},
                               {0.0f, 0.5f}, {from_bits(0x7fc12345), 0.5f},
                               {0.25f, 0.005f}}) {
        auto record = fixture(values.at_50, values.at_54);
        const auto before = snapshot(record);
        CHECK(produce_marker_with_scalar_helpers(record,
              {true, 1.0f, 0, false, 0.125f, std::nullopt}) ==
              MarkerPipelineOutcome::side54_fallback_missing);
        CHECK(snapshot(record) == before);
    }
    auto record = fixture(1.0f, 0.5f);
    CHECK(produce_marker_with_scalar_helpers(record,
          {true, 1.0f, 8, false, std::nullopt, 0.0125f}) ==
          MarkerPipelineOutcome::derived_marker_stored);
    CHECK(side(record, 0x50) == 1.0f && side(record, 0x54) == 0.25f);
    record = fixture(0.5f, 0.0f);
    CHECK(produce_marker_with_scalar_helpers(record,
          {true, 1.0f, 8, false, 0.125f, std::nullopt}) ==
          MarkerPipelineOutcome::derived_marker_stored);
    CHECK(side(record, 0x50) == 0.75f && bits(side(record, 0x54)) == bits(0.0f));
}

void tolerance_bounds_and_fallback_values()
{
    const float below_tolerance = from_bits(0x3c23d709);
    const float tolerance = from_bits(0x3c23d70a);
    const float above_tolerance = from_bits(0x3c23d70b);
    for (float value : {below_tolerance, tolerance, above_tolerance}) {
        auto record = fixture(value, value);
        CHECK(produce_marker_with_scalar_helpers(record,
              {true, 1.0f, 0, false, 0.0f, 0.0f}) ==
              MarkerPipelineOutcome::derived_marker_stored);
        const float expected = value > tolerance ? value : 0.0f;
        CHECK(bits(side(record, 0x50)) == bits(expected));
        CHECK(bits(side(record, 0x54)) == bits(expected));
    }
    struct Case { float old_value; float target; float step; float expected; };
    const float inf = std::numeric_limits<float>::infinity();
    const float nan = from_bits(0x7fc12345);
    for (const auto& test : {Case{0.5f, 1.0f, 100.0f, 1.0f},
                             {0.5f, 0.0f, 100.0f, 0.0f},
                             {0.5f, 1.0f, -1.0f, 0.0f},
                             {0.5f, 0.0f, -1.0f, 1.0f},
                             {0.5f, 1.0f, 0.0f, 0.5f},
                             {0.5f, 1.0f, -0.0f, 0.5f},
                             {-4.0f, 0.0f, 0.0f, 0.0f},
                             {4.0f, 1.0f, 0.0f, 1.0f},
                             {0.5f, 1.0f, nan, 0.0f},
                             {0.5f, 1.0f, inf, 1.0f},
                             {0.5f, 1.0f, -inf, 0.0f},
                             {inf, 1.0f, 0.125f, 0.0f},
                             {-inf, 1.0f, 0.125f, 0.0f},
                             {-0.0f, 1.0f, 0.0f, 0.0f}}) {
        auto record = fixture(test.old_value, test.old_value);
        const auto before = snapshot(record);
        CHECK(produce_marker_with_scalar_helpers(record,
              {true, 1.0f, static_cast<std::uint8_t>(test.target == 1.0f ? 8 : 0),
               test.target == 1.0f, test.step, test.step}) ==
              MarkerPipelineOutcome::derived_marker_stored);
        CHECK(bits(side(record, 0x50)) == bits(test.expected));
        CHECK(bits(side(record, 0x54)) == bits(test.expected));
        check_untouched(record, before, true);
    }
}
} // namespace

int main()
{
    normal_and_target_selection();
    bypasses_need_no_fallback();
    equality_unordered_and_signed_zero();
    unresolved_is_transactional_and_ordered();
    tolerance_bounds_and_fallback_values();
    std::cout << "PASS marker_pipeline: " << assertions << " assertions\n";
}

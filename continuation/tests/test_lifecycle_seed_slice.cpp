#include "rage/lifecycle_seed_slice.hpp"

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

std::uint32_t bits(float value)
{
    std::uint32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
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

Bytes snapshot(const PositionRecord& record)
{
    Bytes bytes;
    std::memcpy(bytes.data(), &record, bytes.size());
    return bytes;
}

PositionRecord fixture()
{
    PositionRecord record;
    Bytes bytes;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<unsigned char>((i * 37 + 11) & 0xff);
    std::memcpy(&record, bytes.data(), bytes.size());
    record.updated_at = 0.0f;
    record.validity_marker = -9.0f;
    set_side(record, 0x50, 0.25f);
    set_side(record, 0x54, 0.5f);
    return record;
}

void check_untouched(const PositionRecord& record, const Bytes& before, bool derived)
{
    const auto after = snapshot(record);
    for (std::size_t i = 0; i < after.size(); ++i) {
        const bool writable = (i >= 0x48 && i < 0x4c) ||
                              (derived && i >= 0x50 && i < 0x58);
        if (!writable)
            CHECK(after[i] == before[i]);
    }
}

void direct_seed_cases()
{
    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    struct Case { float now; float timestamp; std::uint32_t expected; };
    const Case cases[] = {
        {-inf, 0.0f, 0x3f4ccccd}, {-1.0f, 0.0f, 0x3f4ccccd},
        {-0.0f, 0.0f, 0x3f4ccccd}, {0.0f, -0.0f, 0x3f4ccccd},
        {0.5f, 0.0f, 0x3f4ccccd}, {1.0f, 0.0f, 0x3f400000},
        {2.0f, 0.0f, 0x3f000000}, {3.0f, 0.0f, 0x3e800000},
        {4.0f, 0.0f, 0x00000000}, {5.0f, 0.0f, 0x00000000},
        {inf, 0.0f, 0x00000000}, {nan, 0.0f, 0x3f4ccccd},
        {0.0f, nan, 0x3f4ccccd}, {inf, inf, 0x3f4ccccd},
        {11.0f, 10.0f, 0x3f400000},
        {16777216.0f, 16777215.0f, 0x3f400000},
        {std::nextafter(4.0f, 0.0f), 0.0f, 0x33800000},
        {std::nextafter(4.0f, inf), 0.0f, 0x00000000}
    };
    for (const auto& item : cases) {
        auto record = fixture();
        record.updated_at = item.timestamp;
        const auto before = snapshot(record);
        CHECK(produce_marker_from_quarter_decay_seed(record,
              {item.now, false, 8, true, std::nullopt, std::nullopt}) ==
              MarkerPipelineOutcome::direct_seed_stored);
        CHECK(bits(record.validity_marker) == item.expected);
        check_untouched(record, before, false);
    }
}

void derived_seed_and_snapshots()
{
    auto record = fixture();
    const auto before = snapshot(record);
    CHECK(produce_marker_from_quarter_decay_seed(record,
          {1.0f, true, 8, false, 0.125f, 0.0125f}) ==
          MarkerPipelineOutcome::derived_marker_stored);
    CHECK(side(record, 0x50) == 0.625f);
    CHECK(side(record, 0x54) == 0.25f);
    CHECK(record.validity_marker == 0.38671875f);
    check_untouched(record, before, true);

    for (const auto mode : {std::uint8_t{0}, std::uint8_t{8}, std::uint8_t{255}}) {
        for (const bool target54 : {false, true}) {
            record = fixture();
            const auto saved = snapshot(record);
            CHECK(produce_marker_from_quarter_decay_seed(record,
                  {4.0f, true, mode, target54, std::nullopt, std::nullopt}) ==
                  MarkerPipelineOutcome::derived_marker_stored);
            CHECK(side(record, 0x50) == (mode == 8 ? 1.0f : 0.0f));
            CHECK(side(record, 0x54) == (target54 ? 1.0f : 0.0f));
            CHECK(bits(record.validity_marker) == 0);
            check_untouched(record, saved, true);
        }
    }

    record = fixture();
    CHECK(produce_marker_from_quarter_decay_seed(record,
          {1.0f, true, 0, true, 0.125f, 0.0125f}) ==
          MarkerPipelineOutcome::derived_marker_stored);
    CHECK(side(record, 0x50) == 0.125f);
    CHECK(side(record, 0x54) == 0.75f);
    CHECK(record.validity_marker == 0.17578125f);
}

void missing_observations()
{
    auto record = fixture();
    const auto before = snapshot(record);
    CHECK(produce_marker_from_quarter_decay_seed(record,
          {1.0f, true, 8, false, std::nullopt, 0.0125f}) ==
          MarkerPipelineOutcome::side50_fallback_missing);
    CHECK(snapshot(record) == before);
    CHECK(produce_marker_from_quarter_decay_seed(record,
          {1.0f, true, 8, false, 0.125f, std::nullopt}) ==
          MarkerPipelineOutcome::side54_fallback_missing);
    CHECK(snapshot(record) == before);

    set_side(record, 0x50, 1.0f);
    CHECK(produce_marker_from_quarter_decay_seed(record,
          {1.0f, true, 8, false, std::nullopt, 0.0125f}) ==
          MarkerPipelineOutcome::derived_marker_stored);
    CHECK(side(record, 0x50) == 1.0f);
    CHECK(side(record, 0x54) == 0.25f);
    CHECK(record.validity_marker == 0.28125f);
}
} // namespace

int main()
{
    direct_seed_cases();
    derived_seed_and_snapshots();
    missing_observations();
    std::cout << "PASS lifecycle_seed_slice: " << assertions << " assertions\n";
}

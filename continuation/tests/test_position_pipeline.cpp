#include "rage/position_lifecycle.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

using namespace rage::offline;

namespace {
int assertions = 0;
const char* scenario = "";
const char* marker_kind = "";
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
    std::cerr << "FAIL " << scenario << "/" << marker_kind << " line " << __LINE__ \
              << ": " #condition "\n"; std::exit(1); } } while (false)

struct ExpectedRead {
    ReadStatus status;
    bool al;
    std::uint8_t active;
    ActorTransition actor_transition;
};

bool same_vec3(const Vec3& left, const Vec3& right)
{
    return std::memcmp(&left, &right, sizeof(Vec3)) == 0;
}

float opaque_float(const PositionRecord& record, std::size_t offset)
{
    float value;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&record);
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

void run_pipeline(std::uint32_t index, const MarkerProducerInputs& producer,
                  MarkerProduction production, float expected_marker,
                  const ResolvedRead& reader, const ExpectedRead& expected)
{
    PositionStore store;
    auto& record = store.at(index);
    record.active = 0x80;
    record.updated_at = 10.0f;
    record.position = {1.0f, 2.0f, 3.0f};
    record.actor_position = {-4.0f, -5.0f, -6.0f};
    record.validity_marker = 99.0f;
    record.opaque_4c.fill(0xa5);

    CHECK(produce_resolved_validity_marker(record, producer) == production);
    CHECK(std::isnan(expected_marker) ? std::isnan(record.validity_marker)
                                     : record.validity_marker == expected_marker);
    CHECK(record.active == 0x80);
    CHECK(same_vec3(record.position, Vec3{1.0f, 2.0f, 3.0f}));
    CHECK(same_vec3(record.actor_position, Vec3{-4.0f, -5.0f, -6.0f}));
    if (production == MarkerProduction::derived_marker_stored) {
        CHECK(opaque_float(record, 0x50) == 1.0f);
        CHECK(opaque_float(record, 0x54) == 0.0f);
    }

    auto expected_record = record;
    expected_record.active = expected.active;
    const PositionRead read = read_resolved_record(store, index, reader);
    CHECK(read.status == expected.status);
    CHECK(read.record == &record);
    CHECK(record.active == expected.active);
    CHECK(std::memcmp(&record, &expected_record, sizeof(record)) == 0);

    // Normal reader exits: SETB AL for usable; ordinary refusals return zero.
    CHECK(read.status != ReadStatus::outside_supported_domain);
    const bool reader_al = read.status == ReadStatus::usable;
    CHECK(reader_al == expected.al);

    int owner = 0;
    Vec3 sink{7.0f, 8.0f, 9.0f};
    Vec3 expected_sink = sink;
    float marker_output = -77.0f;
    if (expected.actor_transition == ActorTransition::copied_record_position) {
        expected_record.actor_position = record.position;
        expected_sink = record.position;
    }
    const auto result = update_actor_consumer(
        {&owner, read, sink},
        {reader_al, ActorGuardOutcome::passed, false, std::nullopt, &marker_output});
    CHECK(result.transition == expected.actor_transition);
    CHECK(result.actor_owner == &owner);
    CHECK(result.record == &record);
    CHECK(result.actor_position_sink == &sink);
    CHECK(same_vec3(sink, expected_sink));
    CHECK(std::memcmp(&record, &expected_record, sizeof(record)) == 0);
    CHECK(std::memcmp(&marker_output, &record.validity_marker, sizeof(float)) == 0);
}

void zero_and_quiet_nan_policy_chains()
{
    struct Case {
        const char* name;
        std::uint32_t index;
        ResolvedRead reader;
        ReadStatus status;
        bool al;
        std::uint8_t active;
        ActorTransition zero_transition;
        ActorTransition nan_transition;
    };
    const std::array<Case, 8> cases{{
        {"policy_applies_without_slot_flag", 17,
         {true, false, 1, {true, false, 0}, 10.0f},
         ReadStatus::cleared_by_policy, false, 0,
         ActorTransition::inactive_record, ActorTransition::inactive_record},
        {"policy_applies_at_64_with_nonpositive_slot", 64,
         {true, true, 0, {false, true, 1}, 10.0f},
         ReadStatus::cleared_by_policy, false, 0,
         ActorTransition::inactive_record, ActorTransition::inactive_record},
        {"disabled_policy_clears", 17,
         {true, false, 0, {false, false, 0}, 10.0f},
         ReadStatus::cleared_by_policy, false, 0,
         ActorTransition::inactive_record, ActorTransition::inactive_record},
        {"positive_slot_bypasses_policy", 17,
         {true, true, 1, {false, false, 0}, 10.0f},
         ReadStatus::usable, true, 0x80,
         ActorTransition::copied_record_position, ActorTransition::copied_record_position},
        {"index_65_bypasses_policy", 65,
         {true, false, 0, {false, false, 0}, 10.0f},
         ReadStatus::usable, true, 0x80,
         ActorTransition::copied_record_position, ActorTransition::copied_record_position},
        {"source_failure_precedes_policy", 17,
         {false, false, 0, {false, false, 0}, 10.0f},
         ReadStatus::source_unavailable, false, 0x80,
         ActorTransition::marker_gate_rejected, ActorTransition::copied_record_position},
        {"bypassed_policy_expires_at_four", 17,
         {true, true, 1, {false, false, 0}, 14.0f},
         ReadStatus::expired, false, 0x80,
         ActorTransition::marker_gate_rejected, ActorTransition::copied_record_position},
        {"policy_clear_precedes_expiry", 17,
         {true, false, 0, {true, false, 0}, 14.0f},
         ReadStatus::cleared_by_policy, false, 0,
         ActorTransition::inactive_record, ActorTransition::inactive_record},
    }};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (const auto& test : cases) {
        scenario = test.name;
        marker_kind = "zero";
        run_pipeline(test.index, {true, 0.0f, 8, false, std::nullopt},
                     MarkerProduction::derived_marker_stored, 0.0f, test.reader,
                     {test.status, test.al, test.active, test.zero_transition});
        marker_kind = "quiet_nan";
        run_pipeline(test.index, {true, nan, 8, false, std::nullopt},
                     MarkerProduction::derived_marker_stored, nan, test.reader,
                     {test.status, test.al, test.active, test.nan_transition});
    }
}

void signed_marker_normal_return_chains()
{
    marker_kind = "negative";
    scenario = "enabled_policy_accepts_negative_marker";
    run_pipeline(17, {false, -1.0f, 0, false, std::nullopt},
                 MarkerProduction::direct_seed_stored, -1.0f,
                 {true, false, 0, {true, false, 0}, 10.0f},
                 {ReadStatus::usable, true, 0x80, ActorTransition::copied_record_position});
    scenario = "source_failure_keeps_negative_marker";
    run_pipeline(17, {false, -1.0f, 0, false, std::nullopt},
                 MarkerProduction::direct_seed_stored, -1.0f,
                 {false, false, 0, {true, false, 0}, 10.0f},
                 {ReadStatus::source_unavailable, false, 0x80,
                  ActorTransition::marker_gate_rejected});
    scenario = "expired_negative_marker";
    run_pipeline(17, {false, -1.0f, 0, false, std::nullopt},
                 MarkerProduction::direct_seed_stored, -1.0f,
                 {true, false, 0, {true, false, 0}, 14.0f},
                 {ReadStatus::expired, false, 0x80, ActorTransition::marker_gate_rejected});
    marker_kind = "positive";
    scenario = "source_failure_keeps_positive_marker";
    run_pipeline(17, {false, 2.5f, 0, false, std::nullopt},
                 MarkerProduction::direct_seed_stored, 2.5f,
                 {false, true, 1, {true, false, 0}, 10.0f},
                 {ReadStatus::source_unavailable, false, 0x80,
                  ActorTransition::copied_record_position});
}
} // namespace

int main()
{
    zero_and_quiet_nan_policy_chains();
    signed_marker_normal_return_chains();
    std::cout << "position_pipeline: 20 coherent offline scenarios, " << assertions
              << " assertions passed\n";
}

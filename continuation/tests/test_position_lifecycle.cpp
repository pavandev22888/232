#include "rage/position_lifecycle.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <tuple>

using namespace rage::offline;

namespace {
int assertions = 0;
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; \
    std::exit(1); } } while (false)

bool same_vec3(const Vec3& left, const Vec3& right)
{
    return std::memcmp(&left, &right, sizeof(Vec3)) == 0;
}

void set_opaque_float(PositionRecord& record, std::size_t offset, float value)
{
    auto* bytes = reinterpret_cast<unsigned char*>(&record);
    std::memcpy(bytes + offset, &value, sizeof(value));
}

float opaque_float(const PositionRecord& record, std::size_t offset)
{
    float value;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&record);
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

ActorConsumerInputs inputs(bool reader_al = true,
                           ActorGuardOutcome guards = ActorGuardOutcome::passed,
                           bool geometry = false,
                           std::optional<Vec3> trace = std::nullopt,
                           float* marker = nullptr)
{
    return {reader_al, guards, geometry, trace, marker};
}

void reader_pointer_and_actor_identity()
{
    PositionStore store;
    auto& record = store.at(7);
    record.active = 1;
    record.position = {12.0f, 23.0f, 34.0f};
    record.actor_position = {-1.0f, -2.0f, -3.0f};
    record.validity_marker = 2.5f;
    const ResolvedRead read_input{false, true, 1, {true, false, 0}, 1.0f};
    const PositionRead read = read_resolved_record(store, 7, read_input);
    CHECK(read.status == ReadStatus::source_unavailable);
    CHECK(read.record == &record);

    int owner = 0;
    Vec3 actor_position{90.0f, 91.0f, 92.0f};
    float marker_output = -1.0f;
    const ActorRecordBinding binding{&owner, read, actor_position};
    const auto result = update_actor_consumer(
        binding, inputs(true, ActorGuardOutcome::passed, false, std::nullopt, &marker_output));

    CHECK(result.transition == ActorTransition::copied_record_position);
    CHECK(result.actor_owner == &owner);
    CHECK(result.record == read.record);
    CHECK(result.actor_position_sink == &actor_position);
    CHECK(same_vec3(record.actor_position, record.position));
    CHECK(same_vec3(actor_position, record.position));
    CHECK(marker_output == record.validity_marker);
    CHECK(record.validity_marker == 2.5f);
}

void marker_guard_keeps_unordered_behavior()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (const auto& [marker, reader_al, expected] : {
             std::tuple<float, bool, ActorTransition>{0.0f, false,
                 ActorTransition::marker_gate_rejected},
             {-1.0f, false, ActorTransition::marker_gate_rejected},
             {1.0f, false, ActorTransition::copied_record_position},
             {nan, false, ActorTransition::copied_record_position},
             {0.0f, true, ActorTransition::copied_record_position}}) {
        PositionStore store;
        auto& record = store.at(9);
        record.active = 1;
        record.position = {4.0f, 5.0f, 6.0f};
        record.actor_position = {7.0f, 8.0f, 9.0f};
        record.validity_marker = marker;
        const PositionRead read{&record, ReadStatus::usable};
        int owner = 0;
        Vec3 actor_position{30.0f, 31.0f, 32.0f};
        const Vec3 old_actor = actor_position;
        const Vec3 old_record_actor = record.actor_position;
        float marker_output = 77.0f;
        const auto result = update_actor_consumer(
            {&owner, read, actor_position},
            inputs(reader_al, ActorGuardOutcome::passed, false, std::nullopt, &marker_output));

        CHECK(result.transition == expected);
        CHECK(marker_output == marker || (std::isnan(marker) &&
              std::isnan(marker_output)));
        if (expected == ActorTransition::marker_gate_rejected) {
            CHECK(same_vec3(actor_position, old_actor));
            CHECK(same_vec3(record.actor_position, old_record_actor));
        } else {
            CHECK(same_vec3(actor_position, record.position));
            CHECK(same_vec3(record.actor_position, record.position));
        }
    }
}

void distance_and_resolved_trace_paths()
{
    PositionStore store;
    auto& record = store.at(11);
    record.active = 1;
    record.position = {0.0f, 0.0f, 0.0f};
    record.actor_position = {3.0f, 4.0f, 0.0f};
    record.validity_marker = 5.0f;
    const PositionRead read{&record, ReadStatus::usable};
    int owner = 0;
    Vec3 actor_position{100.0f, 100.0f, 100.0f};
    const Vec3 trace_position{20.0f, 30.0f, 40.0f};
    auto result = update_actor_consumer(
        {&owner, read, actor_position},
        inputs(true, ActorGuardOutcome::passed, true, trace_position));
    CHECK(result.transition == ActorTransition::traced_position_applied);
    CHECK(result.record == &record && result.actor_position_sink == &actor_position);
    CHECK(same_vec3(record.actor_position, trace_position));
    CHECK(same_vec3(actor_position, trace_position));
    CHECK(record.validity_marker == 5.0f);

    record.position = {1.0f, 2.0f, 3.0f};
    record.actor_position = {101.0f, 2.0f, 3.0f};
    result = update_actor_consumer(
        {&owner, read, actor_position},
        inputs(true, ActorGuardOutcome::passed, true, Vec3{9.0f, 9.0f, 9.0f}));
    CHECK(result.transition == ActorTransition::copied_record_position);
    CHECK(same_vec3(record.actor_position, record.position));
    CHECK(same_vec3(actor_position, record.position));

    record.actor_position.x = std::numeric_limits<float>::quiet_NaN();
    const Vec3 unordered_trace{13.0f, 14.0f, 15.0f};
    result = update_actor_consumer(
        {&owner, read, actor_position},
        inputs(true, ActorGuardOutcome::passed, true, unordered_trace));
    CHECK(result.transition == ActorTransition::traced_position_applied);
    CHECK(same_vec3(record.actor_position, unordered_trace));

    record.actor_position = {1.0f, 1.0f, 1.0f};
    actor_position = {6.0f, 6.0f, 6.0f};
    const Vec3 old_record_actor = record.actor_position;
    const Vec3 old_actor = actor_position;
    result = update_actor_consumer(
        {&owner, read, actor_position}, inputs(true, ActorGuardOutcome::passed, true));
    CHECK(result.transition == ActorTransition::trace_observation_missing);
    CHECK(same_vec3(record.actor_position, old_record_actor));
    CHECK(same_vec3(actor_position, old_actor));
}

void early_gates_and_missing_record()
{
    PositionStore store;
    auto& record = store.at(13);
    record.active = 1;
    record.position = {1.0f, 2.0f, 3.0f};
    record.actor_position = {4.0f, 5.0f, 6.0f};
    record.validity_marker = 3.0f;
    const PositionRead read{&record, ReadStatus::usable};
    int owner = 0;
    Vec3 actor_position{7.0f, 8.0f, 9.0f};
    float marker_output = 12.0f;
    auto result = update_actor_consumer(
        {&owner, read, actor_position},
        inputs(true, ActorGuardOutcome::exit_copy_marker, false, std::nullopt, &marker_output));
    CHECK(result.transition == ActorTransition::guard_exit_copy_marker);
    CHECK(marker_output == record.validity_marker);

    const Vec3 old_actor = actor_position;
    const Vec3 old_record_actor = record.actor_position;
    result = update_actor_consumer(
        {&owner, read, actor_position},
        inputs(true, ActorGuardOutcome::exit_clear_marker, false,
               std::nullopt, &marker_output));
    CHECK(result.transition == ActorTransition::guard_exit_clear_marker);
    CHECK(marker_output == 0.0f);
    CHECK(record.validity_marker == 3.0f);
    CHECK(same_vec3(actor_position, old_actor));
    CHECK(same_vec3(record.actor_position, old_record_actor));

    result = update_actor_consumer(
        {&owner, read, actor_position},
        inputs(true, ActorGuardOutcome::exit_clear_marker));
    CHECK(result.transition == ActorTransition::guard_exit_clear_marker);
    CHECK(record.validity_marker == 3.0f);
    CHECK(same_vec3(actor_position, old_actor));
    CHECK(same_vec3(record.actor_position, old_record_actor));

    record.active = 0;
    result = update_actor_consumer(
        {&owner, read, actor_position}, inputs(true, ActorGuardOutcome::passed, false));
    CHECK(result.transition == ActorTransition::inactive_record);

    const PositionRead missing{nullptr, ReadStatus::outside_supported_domain};
    marker_output = 99.0f;
    result = update_actor_consumer(
        {&owner, missing, actor_position},
        inputs(true, ActorGuardOutcome::passed, false, std::nullopt, &marker_output));
    CHECK(result.transition == ActorTransition::no_record);
    CHECK(result.actor_owner == &owner && result.record == nullptr);
    CHECK(result.actor_position_sink == &actor_position);
    CHECK(marker_output == 0.0f);
}

void marker_producer_and_connected_chain()
{
    PositionStore store;
    auto& record = store.at(17);
    set_opaque_float(record, 0x50, 9.0f);
    set_opaque_float(record, 0x54, -3.0f);

    auto effect = produce_resolved_validity_marker(
        record, {false, 0.75f, 3, false, std::nullopt});
    CHECK(effect == MarkerProduction::direct_seed_stored);
    CHECK(record.validity_marker == 0.75f);
    CHECK(opaque_float(record, 0x50) == 9.0f);
    CHECK(opaque_float(record, 0x54) == -3.0f);

    record.validity_marker = -4.0f;
    effect = produce_resolved_validity_marker(
        record, {true, 0.5f, 8, false, std::nullopt});
    CHECK(effect == MarkerProduction::trace_observation_missing);
    CHECK(record.validity_marker == -4.0f);
    CHECK(opaque_float(record, 0x50) == 9.0f);
    CHECK(opaque_float(record, 0x54) == -3.0f);

    effect = produce_resolved_validity_marker(
        record, {true, 0.5f, 8, false, MarkerSideValues{0.5f, 0.25f}});
    CHECK(effect == MarkerProduction::derived_marker_stored);
    CHECK(opaque_float(record, 0x50) == 0.5f);
    CHECK(opaque_float(record, 0x54) == 0.25f);
    CHECK(record.validity_marker == 0.28125f);

    record.active = 1;
    record.updated_at = 10.0f;
    record.position = {1.0f, 2.0f, 3.0f};
    record.actor_position = {-1.0f, -2.0f, -3.0f};
    const PositionRead read = read_resolved_record(
        store, 17, {true, false, 0, {true, false, 0}, 10.0f});
    CHECK(read.status == ReadStatus::usable);
    CHECK(read.record == &record);
    int owner = 0;
    Vec3 actor_position{9.0f, 8.0f, 7.0f};
    const auto result = update_actor_consumer(
        {&owner, read, actor_position}, inputs(false, ActorGuardOutcome::passed, false));
    CHECK(result.transition == ActorTransition::copied_record_position);
    CHECK(result.record == read.record && result.actor_owner == &owner);
    CHECK(same_vec3(record.actor_position, record.position));
    CHECK(same_vec3(actor_position, record.position));

    effect = produce_resolved_validity_marker(
        record, {true, 0.0f, 8, true, std::nullopt});
    CHECK(effect == MarkerProduction::derived_marker_stored);
    CHECK(opaque_float(record, 0x50) == 1.0f);
    CHECK(opaque_float(record, 0x54) == 1.0f);
    CHECK(record.validity_marker == 0.0f);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    effect = produce_resolved_validity_marker(
        record, {true, nan, 7, false, std::nullopt});
    CHECK(effect == MarkerProduction::derived_marker_stored);
    CHECK(opaque_float(record, 0x50) == 0.0f);
    CHECK(opaque_float(record, 0x54) == 0.0f);
    CHECK(std::isnan(record.validity_marker));
}
} // namespace

int main()
{
    reader_pointer_and_actor_identity();
    marker_guard_keeps_unordered_behavior();
    distance_and_resolved_trace_paths();
    early_gates_and_missing_record();
    marker_producer_and_connected_chain();
    std::cout << "position_lifecycle: " << assertions << " assertions passed\n";
}

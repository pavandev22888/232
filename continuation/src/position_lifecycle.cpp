#include "rage/position_lifecycle.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>

namespace rage::offline {
namespace {
constexpr float actor_fallback_distance = 100.0f; // 0x4206CDC0
constexpr float marker_side50_scale = -0.5f; // 0x4205A530
constexpr float side50_age_scale = 0x1.555556p+0f; // 0x42074CF4
constexpr float side50_age_limit = 0.75f; // 0x4206DB58

// These float words stay opaque in the shared record header.
constexpr std::size_t side50_reference_offset = offsetof(PositionRecord, opaque_10) + 4;
constexpr std::size_t marker_side50_offset = offsetof(PositionRecord, opaque_4c) + 4;
constexpr std::size_t marker_side54_offset = offsetof(PositionRecord, opaque_4c) + 8;
static_assert(marker_side50_offset == 0x50);
static_assert(marker_side54_offset == 0x54);
static_assert(side50_reference_offset == 0x14);

void write_opaque_float(PositionRecord& record, std::size_t offset, float value)
{
    auto* bytes = reinterpret_cast<unsigned char*>(&record);
    std::memcpy(bytes + offset, &value, sizeof(value));
}

float read_opaque_float(const PositionRecord& record, std::size_t offset)
{
    float value;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&record);
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

float side50_age_output(const PositionRecord& record, const ActorSide50Output& output)
{
    if (!output.age_ramp_enabled)
        return 1.0f;
    const float elapsed = output.now - read_opaque_float(record, side50_reference_offset);
    const float scaled = side50_age_scale * elapsed;
    // CMPLTSS masks preserve unordered scaled values and nonnegative signed zero.
    const float nonnegative = elapsed < 0.0f ? 0.0f : scaled;
    return side50_age_limit < elapsed ? 1.0f : nonnegative;
}

float maxss_output(float record_value, float output_value)
{
    // MAXSS selects its second operand on equality or unordered, unlike fmax.
    return record_value > output_value ? record_value : output_value;
}

bool zero_ge_marker(float marker)
{
    // UCOMISS/JAE at 414FD90C exits for ordered 0 >= N+48, not unordered.
    return !std::isnan(marker) && 0.0f >= marker;
}

float actor_record_distance(const Vec3& actor, const Vec3& record)
{
    const float dx = actor.x - record.x;
    const float dy = actor.y - record.y;
    const float dz = actor.z - record.z;
    const float x2 = dx * dx;
    const float y2 = dy * dy;
    const float z2 = dz * dz;
    return std::sqrt((x2 + y2) + z2);
}

bool distance_takes_fallback(float distance)
{
    // JAE after UCOMISS: unordered distances continue into the trace path.
    return !std::isnan(distance) && distance >= actor_fallback_distance;
}
} // namespace

ActorConsumerResult update_actor_consumer(const ActorRecordBinding& binding,
                                          const ActorConsumerInputs& inputs)
{
    PositionRecord* record = binding.reader_output.record;
    const auto finish = [&](ActorTransition transition, bool clear_marker = false) {
        if (inputs.marker_output) {
            if (record && !clear_marker) {
                if (inputs.marker_output != &record->validity_marker)
                    std::memcpy(inputs.marker_output, &record->validity_marker,
                                sizeof(record->validity_marker));
            } else {
                *inputs.marker_output = 0.0f;
            }
        }
        if (record && !clear_marker && inputs.side50_output.value &&
            transition != ActorTransition::trace_observation_missing) {
            *inputs.side50_output.value = maxss_output(
                read_opaque_float(*record, marker_side50_offset),
                *inputs.side50_output.value);
        }
        return ActorConsumerResult{transition, binding.actor_owner, record,
                                   &binding.actor_position_sink};
    };

    if (!record)
        return finish(ActorTransition::no_record);
    if (inputs.guard_outcome == ActorGuardOutcome::exit_copy_marker)
        return finish(ActorTransition::guard_exit_copy_marker);
    if (inputs.guard_outcome == ActorGuardOutcome::exit_clear_marker)
        return finish(ActorTransition::guard_exit_clear_marker, true);
    if (record->active == 0)
        return finish(ActorTransition::inactive_record);
    if (!inputs.reader_result_al && zero_ge_marker(record->validity_marker))
        return finish(ActorTransition::marker_gate_rejected);

    const float distance = actor_record_distance(record->actor_position,
                                                 record->position);
    const bool use_record_position = !inputs.geometry_option_enabled ||
                                     distance_takes_fallback(distance);
    if (!use_record_position && !inputs.traced_position)
        return finish(ActorTransition::trace_observation_missing);

    const Vec3 next = use_record_position ? record->position
                                          : *inputs.traced_position;
    record->actor_position = next;
    binding.actor_position_sink = next;
    if (inputs.side50_output.value)
        *inputs.side50_output.value = side50_age_output(*record, inputs.side50_output);
    return finish(use_record_position ? ActorTransition::copied_record_position
                                      : ActorTransition::traced_position_applied);
}

MarkerProduction produce_resolved_validity_marker(
    PositionRecord& record, const MarkerProducerInputs& inputs)
{
    if (!inputs.derived_branch_taken) {
        record.validity_marker = inputs.marker_seed;
        return MarkerProduction::direct_seed_stored;
    }

    float side50;
    float side54;
    const bool ordered_nonzero_seed = !std::isnan(inputs.marker_seed) &&
                                      inputs.marker_seed != 0.0f;
    if (ordered_nonzero_seed) {
        if (!inputs.traced_side_values)
            return MarkerProduction::trace_observation_missing;
        side50 = inputs.traced_side_values->at_50;
        side54 = inputs.traced_side_values->at_54;
    } else {
        side50 = inputs.mode_byte == 8 ? 1.0f : 0.0f;
        side54 = inputs.side54_seed_is_one ? 1.0f : 0.0f;
    }

    write_opaque_float(record, marker_side50_offset, side50);
    write_opaque_float(record, marker_side54_offset, side54);
    const float field50 = read_opaque_float(record, marker_side50_offset);
    const float field54 = read_opaque_float(record, marker_side54_offset);
    const float scaled_side50 = field50 * marker_side50_scale;
    const float one_plus_scaled = scaled_side50 + 1.0f;
    const float one_minus_side54 = 1.0f - field54;
    const float seeded = one_minus_side54 * inputs.marker_seed;
    record.validity_marker = seeded * one_plus_scaled;
    return MarkerProduction::derived_marker_stored;
}

} // namespace rage::offline

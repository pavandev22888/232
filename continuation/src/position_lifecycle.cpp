#include "rage/position_lifecycle.hpp"

#include <cmath>
#include <cstring>

namespace rage::offline {
namespace {
constexpr float actor_fallback_distance = 100.0f; // 0x4206CDC0

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
    const auto finish = [&](ActorTransition transition) {
        if (inputs.marker_output) {
            if (record) {
                if (inputs.marker_output != &record->validity_marker)
                    std::memcpy(inputs.marker_output, &record->validity_marker,
                                sizeof(record->validity_marker));
            } else {
                *inputs.marker_output = 0.0f;
            }
        }
        return ActorConsumerResult{transition, binding.actor_owner, record,
                                   &binding.actor_position_sink};
    };

    if (!record)
        return finish(ActorTransition::no_record);
    if (!inputs.actor_guards_passed)
        return finish(ActorTransition::actor_guards_rejected);
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
    return finish(use_record_position ? ActorTransition::copied_record_position
                                      : ActorTransition::traced_position_applied);
}

} // namespace rage::offline

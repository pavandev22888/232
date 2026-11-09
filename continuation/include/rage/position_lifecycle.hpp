#ifndef RAGE_POSITION_LIFECYCLE_HPP
#define RAGE_POSITION_LIFECYCLE_HPP

#include "rage/position_records.hpp"

#include <optional>

namespace rage::offline {

enum class ActorTransition {
    no_record,
    actor_guards_rejected,
    inactive_record,
    marker_gate_rejected,
    copied_record_position,
    traced_position_applied,
    trace_observation_missing
};

// Caller-resolved association. The owner pointer is opaque; no actor ID is inferred.
struct ActorRecordBinding {
    const void* actor_owner;
    const PositionRead& reader_output;
    Vec3& actor_position_sink;
};

struct ActorConsumerInputs {
    bool reader_result_al;
    bool actor_guards_passed;
    bool geometry_option_enabled;
    std::optional<Vec3> traced_position;
    float* marker_output;
};

struct ActorConsumerResult {
    ActorTransition transition;
    const void* actor_owner;
    PositionRecord* record;
    Vec3* actor_position_sink;
};

// Replays the bounded consumer slice at 414FD8xx..414FDB37 after resolved guards.
ActorConsumerResult update_actor_consumer(const ActorRecordBinding& binding,
                                          const ActorConsumerInputs& inputs);

} // namespace rage::offline
#endif

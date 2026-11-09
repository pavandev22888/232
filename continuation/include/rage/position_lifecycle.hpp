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
    bool reader_result_al; // Not derived from PositionRead::status.
    bool actor_guards_passed; // Resolved virtual/opaque guards.
    bool geometry_option_enabled; // Selected configuration byte is nonzero.
    std::optional<Vec3> traced_position; // Resolved three-axis provider result.
    float* marker_output; // Optional N+48 output.
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

enum class MarkerProduction {
    direct_seed_stored,
    derived_marker_stored,
    trace_observation_missing
};

struct MarkerSideValues {
    // Raw float observations for the opaque N+50/N+54 words.
    float at_50;
    float at_54;
};

struct MarkerProducerInputs {
    bool derived_branch_taken; // [ebp+0xc] != 0.
    float marker_seed; // Low XMM4 value at the local producer tail.
    std::uint8_t mode_byte; // BL; 8 seeds N+50 with 1.0.
    bool side54_seed_is_one; // Resolved XMM3 seed before the provider calls.
    std::optional<MarkerSideValues> traced_side_values;
};

// Replays the local N+48 producer tail at 414F868B..414F87BD.
MarkerProduction produce_resolved_validity_marker(
    PositionRecord& record, const MarkerProducerInputs& inputs);

} // namespace rage::offline
#endif

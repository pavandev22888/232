#ifndef RAGE_POSITION_LIFECYCLE_HPP
#define RAGE_POSITION_LIFECYCLE_HPP

#include "rage/position_records.hpp"

#include <optional>

namespace rage::offline {

enum class ActorTransition {
    no_record,
    guard_exit_copy_marker,
    guard_exit_clear_marker,
    inactive_record,
    marker_gate_rejected,
    copied_record_position,
    resolved_position_applied,
    provider_observation_missing
};

// Caller-resolved association. The owner pointer is opaque; no actor ID is inferred.
struct ActorRecordBinding {
    const void* actor_owner;
    const PositionRead& reader_output;
    Vec3& actor_position_sink;
};

// Resolved normal exits only; opaque dispatch and index faults are outside this model.
enum class ActorGuardOutcome {
    passed,
    exit_copy_marker,
    exit_clear_marker
};

struct ActorSide50Output {
    // External initialized scalar, disjoint from N and both position sinks.
    // May share the external scalar used by marker_output.
    float* value = nullptr;
    bool age_ramp_enabled = false; // Resolved selected cfg +274/+275 byte.
    float now = 0.0f; // Resolved float behind 0x4223CA04.
};

struct ActorConsumerInputs {
    bool reader_result_al; // Not derived from PositionRead::status.
    ActorGuardOutcome guard_outcome;
    bool position_adjustment_enabled; // Selected configuration byte is nonzero.
    std::optional<Vec3> resolved_position; // Resolved three-axis provider result.
    float* marker_output; // Optional N+48 output.
    ActorSide50Output side50_output{};
};

struct ActorConsumerResult {
    ActorTransition transition;
    const void* actor_owner;
    PositionRecord* record;
    Vec3* actor_position_sink;
};

// Replays the bounded consumer slice at 414FD8xx..414FDB4C after resolved guards.
// Missing provider observations skip side50 stores (not an original exit).
ActorConsumerResult update_actor_consumer(const ActorRecordBinding& binding,
                                          const ActorConsumerInputs& inputs);

enum class MarkerProduction {
    direct_seed_stored,
    derived_marker_stored,
    provider_observation_missing
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
    std::optional<MarkerSideValues> resolved_side_values;
};

// Replays the local N+48 producer tail at 414F868B..414F87BD.
MarkerProduction produce_resolved_validity_marker(
    PositionRecord& record, const MarkerProducerInputs& inputs);

} // namespace rage::offline
#endif

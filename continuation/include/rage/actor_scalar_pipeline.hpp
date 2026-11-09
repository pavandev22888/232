#ifndef RAGE_ACTOR_SCALAR_PIPELINE_HPP
#define RAGE_ACTOR_SCALAR_PIPELINE_HPP

#include "rage/position_records.hpp"

#include <optional>
#include <variant>

namespace rage::offline {

struct ActorScalarAxisObservation {
    float tolerance;
    std::optional<float> fallback_step;
};

struct ActorScalarObservations {
    ActorScalarAxisObservation x;
    ActorScalarAxisObservation y;
    ActorScalarAxisObservation z;
};

enum class ActorScalarAxis { x, y, z };

struct ActorScalarUnresolved {
    ActorScalarAxis axis;
};

using ActorScalarResult = std::variant<Vec3, ActorScalarUnresolved>;

// Snapshots old N+30/+34/+38 and targets N+24/+28/+2C; observations are per call.
// Uses the scalar helper's bounded FP domain. No gates, consumer calls or stores.
// Unresolved returns the first missing required fallback, never a partial vector.
ActorScalarResult resolve_actor_scalar_position(
    const PositionRecord& record, ActorScalarObservations observations);

} // namespace rage::offline
#endif

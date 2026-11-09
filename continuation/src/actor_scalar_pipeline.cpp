#include "rage/actor_scalar_pipeline.hpp"
#include "rage/resolved_scalar_helper.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace rage::offline {
namespace {
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(offsetof(PositionRecord, position) == 0x24);
static_assert(offsetof(PositionRecord, actor_position) == 0x30);
static_assert(offsetof(Vec3, x) == 0 && offsetof(Vec3, y) == 4 &&
              offsetof(Vec3, z) == 8 && sizeof(Vec3) == 12);
} // namespace

ActorScalarResult resolve_actor_scalar_position(
    const PositionRecord& record, ActorScalarObservations observations)
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(&record);
    Vec3 next;
    Vec3 target;
    std::memcpy(&next, bytes + offsetof(PositionRecord, actor_position), sizeof(next));
    std::memcpy(&target, bytes + offsetof(PositionRecord, position), sizeof(target));

    const std::uint32_t divisor_bits = 0x3DAE147B;
    float divisor;
    std::memcpy(&divisor, &divisor_bits, sizeof(divisor));
    const float bound = std::numeric_limits<float>::max();
    const std::array<float*, 3> values{{&next.x, &next.y, &next.z}};
    const std::array<float, 3> targets{{target.x, target.y, target.z}};
    const std::array<ActorScalarAxisObservation, 3> per_axis{{
        observations.x, observations.y, observations.z}};
    const std::array<ActorScalarAxis, 3> axes{{
        ActorScalarAxis::x, ActorScalarAxis::y, ActorScalarAxis::z}};

    for (std::size_t i = 0; i < values.size(); ++i) {
        const ScalarHelperInputs inputs{targets[i], divisor, -bound, bound,
            per_axis[i].tolerance, -1.0f, per_axis[i].fallback_step};
        if (update_resolved_scalar(*values[i], inputs) ==
            ScalarHelperEffect::fallback_step_missing)
            return ActorScalarUnresolved{axes[i]};
    }
    return next;
}

} // namespace rage::offline

#ifndef RAGE_BOUNDED_AGGREGATION_HPP
#define RAGE_BOUNDED_AGGREGATION_HPP

#include <cstdint>
#include <optional>

namespace rage::offline {

struct TargetScoreProjection {
    std::uint32_t callback_count_bits;
    float weighted_sum;
};

struct ScorePointFields {
    std::uint32_t field_1c;
    float recorded_chance_34;
    float damage_3c;
};

// Only the counter/sum prefix through 4149E983, on the direct +1C=2/3 path.
// Other +1C values return unresolved, not an unchanged original state.
std::optional<TargetScoreProjection> accumulate_direct_score_prefix(
    const TargetScoreProjection& before, const ScorePointFields& point);

// 414A8D96..414A8DBB: nullopt means no T+F0 write (signed count <= 0).
// Requires an observed complete projection at that block, not a full target.
std::optional<std::int32_t> target_score_f0_write(
    const TargetScoreProjection& projection);

// Both APIs require binary32, round-to-nearest, no FTZ/DAZ or fast-math.
// sNaNs, NaN payload selection and observable FP traps/flags are out of scope.
} // namespace rage::offline
#endif

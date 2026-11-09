#ifndef RAGE_BOUNDED_AGGREGATION_HPP
#define RAGE_BOUNDED_AGGREGATION_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

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

struct CallbackOwnerFields {
    std::uint8_t field_c0;
    std::uint32_t field_8c;
    std::uint32_t field_a0_bits;
    std::uint8_t field_138;
};

struct CallbackPointFields {
    ScorePointFields score;
    std::uint8_t field_25;
    std::int32_t field_28;
    std::uint32_t field_30_bits;
};

struct CallbackSettingsObservation {
    std::uint32_t index;
    std::optional<std::uint32_t> selected_flags;
};

enum class CallbackPrefixStop {
    score_tail_4149e988,
    return_4149eb28,
    settings_index_unresolved,
    settings_flags_unresolved,
    unknown_index_handler_4149ec2d
};

struct CallbackPrefixResult {
    TargetScoreProjection after;
    CallbackPrefixStop stop;
    std::optional<std::uint8_t> tail_cl;
};

// Entry at 4149E790; every result retains INC, including unresolved paths.
// Settings are observations of the actual pointer chain, not a provider.
CallbackPrefixResult accumulate_callback_score_prefix(
    const TargetScoreProjection& before, const CallbackPointFields& point,
    const CallbackOwnerFields& owner, std::uint8_t accumulator_06,
    std::uint8_t accumulator_07,
    const std::optional<CallbackSettingsObservation>& settings);

struct CallbackTailStatistics {
    float field_0c;
    float field_10;
    float field_14;
};

// Requires arrival at 4149E988; stops before owner-dependent ranking at E9BD.
CallbackTailStatistics update_callback_tail_statistics(
    CallbackTailStatistics before, const CallbackPointFields& point);

struct CallbackInitialPoint {
    std::array<std::uint8_t, 0x50> owner_70_bf;
    std::uint8_t owner_c0;
};

// Entry at E9BD with owner+C0==0: copies point bytes, then writes C0=1.
// Nonzero C0 is unresolved ranking, not a proved no-op.
std::optional<CallbackInitialPoint> initialize_callback_point(
    std::uint8_t owner_c0, const std::array<std::uint8_t, 0x50>& point);

enum class CallbackRankStop {
    return_4149eb28,
    owner_flags_b0_unresolved
};

struct CallbackRankResult {
    CallbackInitialPoint after;
    CallbackRankStop stop;
    bool replaced;
};

// Entry at E9BD after score/statistics; +4->+B0 is an observation, not a provider.
// Point and owner bytes are little-endian snapshots; no aliasing/concurrent writes.
CallbackRankResult rank_callback_point(
    const CallbackInitialPoint& before,
    const std::array<std::uint8_t, 0x50>& point,
    std::uint8_t accumulator_05, std::uint8_t accumulator_08,
    std::uint8_t tail_cl, std::optional<std::uint8_t> owner_flags_b0);

// 414A8361..414A838C: six ordered DWORD stores in caller S+00..7F.
// Returns a raw projection, not a full caller object; pointer fields remain raw32.
std::array<std::uint8_t, 0x80> initialize_callback_context32(
    const std::array<std::uint8_t, 0x80>& before, std::uint32_t caller_address);

struct CallbackRaw32Region {
    std::uint32_t address;
    std::vector<std::uint8_t> bytes;
};

struct CallbackMissingRead32 {
    std::uint32_t address;
    std::uint8_t width;
};

enum class CallbackSettingsReadStop {
    complete,
    memory_unobserved,
    index_handler_4149ec2d
};

struct CallbackSettingsResolution {
    CallbackSettingsReadStop stop;
    std::optional<CallbackSettingsObservation> observation;
    std::optional<CallbackMissingRead32> missing;
};

// Interior settings-load block E7DF/E878/E8F4, not callback entry.
// Stable, disjoint, non-wrapping regions; addresses/arithmetic are raw x86 DWORDs.
// No host-pointer dereference, default provider, selector clamp or original call.
CallbackSettingsResolution resolve_callback_settings32(
    const std::vector<CallbackRaw32Region>& memory);

struct CallbackOwnerFlagsResolution {
    std::optional<std::uint8_t> flags;
    std::optional<CallbackMissingRead32> missing;
};

// Entry at a reached E9D4/EBA4 flags read; owner is observed ESI, not caller S.
// Reads DWORD[owner+4], then BYTE[pointee+B0], under the same snapshot contract.
CallbackOwnerFlagsResolution resolve_callback_owner_flags32(
    const std::vector<CallbackRaw32Region>& memory, std::uint32_t owner);

// The floating-point APIs require binary32, round-to-nearest, no FTZ/DAZ or fast-math.
// sNaNs, NaN payload selection and observable FP traps/flags are out of scope.
} // namespace rage::offline
#endif

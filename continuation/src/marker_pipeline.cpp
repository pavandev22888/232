#include "rage/marker_pipeline.hpp"
#include "rage/resolved_scalar_helper.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>

namespace rage::offline {
namespace {
constexpr std::size_t marker_offset = offsetof(PositionRecord, validity_marker);
constexpr std::size_t side50_offset = offsetof(PositionRecord, opaque_4c) + 4;
constexpr std::size_t side54_offset = offsetof(PositionRecord, opaque_4c) + 8;
static_assert(marker_offset == 0x48);
static_assert(side50_offset == 0x50);
static_assert(side54_offset == 0x54);
static_assert(sizeof(float) == 4);

float read_side(const PositionRecord& record, std::size_t offset)
{
    float value;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&record);
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}
} // namespace

MarkerPipelineOutcome produce_marker_with_scalar_helpers(
    PositionRecord& record, MarkerPipelineInputs inputs)
{
    PositionRecord pending;
    std::memcpy(&pending, &record, sizeof(pending));
    MarkerProducerInputs producer{inputs.derived_branch_taken, inputs.marker_seed,
                                 inputs.mode_byte, inputs.side54_seed_is_one,
                                 std::nullopt};
    if (inputs.derived_branch_taken && !std::isnan(inputs.marker_seed) &&
        inputs.marker_seed != 0.0f) {
        float side50 = read_side(pending, side50_offset);
        float side54 = read_side(pending, side54_offset);
        const float target50 = inputs.mode_byte == 8 ? 1.0f : 0.0f;
        const float target54 = inputs.side54_seed_is_one ? 1.0f : 0.0f;
        // 414F8753, then 414F877A; constants at 4209BA60/4209BAA0.
        if (update_resolved_scalar(side50,
                {target50, 0.25f, 0.0f, 1.0f, 0.01f, -1.0f,
                 inputs.fallback_step50}) == ScalarHelperEffect::fallback_step_missing)
            return MarkerPipelineOutcome::side50_fallback_missing;
        if (update_resolved_scalar(side54,
                {target54, 0.025f, 0.0f, 1.0f, 0.01f, -1.0f,
                 inputs.fallback_step54}) == ScalarHelperEffect::fallback_step_missing)
            return MarkerPipelineOutcome::side54_fallback_missing;
        producer.resolved_side_values = MarkerSideValues{side50, side54};
    }

    const auto production = produce_resolved_validity_marker(pending, producer);
    auto* bytes = reinterpret_cast<unsigned char*>(&record);
    const auto* result = reinterpret_cast<const unsigned char*>(&pending);
    std::memcpy(bytes + marker_offset, result + marker_offset, sizeof(float));
    if (inputs.derived_branch_taken) {
        std::memcpy(bytes + side50_offset, result + side50_offset, sizeof(float));
        std::memcpy(bytes + side54_offset, result + side54_offset, sizeof(float));
    }
    return production == MarkerProduction::direct_seed_stored
               ? MarkerPipelineOutcome::direct_seed_stored
               : MarkerPipelineOutcome::derived_marker_stored;
}

} // namespace rage::offline

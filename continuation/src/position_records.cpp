#include "rage/position_records.hpp"

#include <cmath>
#include <limits>

namespace rage::offline {
namespace {
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);

bool ordered_not_equal(float left, float right)
{
    // UCOMISS/JNE: unordered sets ZF, unlike the C++ != operator.
    return !std::isnan(left) && !std::isnan(right) && left != right;
}

bool below_or_unordered(float left, float right)
{
    return std::isnan(left) || std::isnan(right) || left < right;
}

bool snapshot_differs(const Vec3& position, const Vec3& snapshot)
{
    return ordered_not_equal(position.x, snapshot.x) ||
           ordered_not_equal(position.y, snapshot.y) ||
           ordered_not_equal(position.z, snapshot.z);
}

std::uint32_t flags_from_trace(float fraction, std::uint32_t provided)
{
    const std::uint32_t differs_from_one = ordered_not_equal(1.0f, fraction);
    const std::uint32_t before_half = 0.5f > fraction;
    const std::uint32_t at_or_before_half = 0.5f >= fraction;
    const auto keep_mask = (2u * differs_from_one - 2u) |
                           (3u * at_or_before_half - 3u);
    return keep_mask & ((differs_from_one + 2u * before_half) | provided);
}
} // namespace

PositionStore::PositionStore()
{
    for (auto& record : records_)
        record.tag = 100;
}

UpdateEffect update_resolved_record(PositionRecord& record,
                                    const ResolvedUpdate& update,
                                    const TraceObservation& trace)
{
    if (!ordered_not_equal(record.updated_at, update.now) &&
        record.update_type > update.type)
        return UpdateEffect::higher_type_at_same_time;
    if (update.type == 2 && record.sequence > update.sequence)
        return UpdateEffect::older_type_two_sequence;
    if (trace.all_solid)
        return UpdateEffect::all_solid;
    if (trace.start_solid)
        return UpdateEffect::start_solid;
    if (!ordered_not_equal(trace.fraction, 1.0f))
        return UpdateEffect::full_or_unordered_trace;

    record.active = 1;
    // Snapshot the old position before installing the incoming coordinates.
    if (snapshot_differs(record.position, record.snapshot_position)) {
        if (record.snapshot_sequence != update.sequence)
            record.previous_snapshot_sequence = record.snapshot_sequence;
        if (record.snapshot_sequence != update.sequence ||
            record.snapshot_type < update.type) {
            record.snapshot_type = update.type;
            record.snapshot_position = record.position;
            record.snapshot_tick = update.tick;
        }
        record.snapshot_sequence = update.sequence;
    }
    if (record.sequence != update.sequence)
        record.previous_sequence = record.sequence;
    record.sequence = update.sequence;

    const float lookback = update.interval * -256.0f;
    const float boundary = update.now + lookback;
    if (!(boundary >= record.updated_at))
        record.update_gap = update.now - record.updated_at;
    record.updated_at = update.now;

    const bool use_trace_end = below_or_unordered(trace.fraction, 0.97f) &&
        (static_cast<std::uint32_t>(update.type) & ~4u) != 1u;
    record.position = use_trace_end ? trace.end_position : update.position;
    record.flags = flags_from_trace(trace.fraction, trace.provided_flags);
    record.update_type = update.type;
    record.updated_tick = update.tick;
    if (update.tag >= 0)
        record.tag = update.tag;
    return UpdateEffect::applied;
}

void store_resolved_item_definition(PositionRecord& record,
                                    std::optional<std::uint16_t> item_word)
{
    if (item_word) {
        const std::int32_t word = *item_word;
        record.item_definition = word >= 0x8000 ? word - 0x10000 : word;
    }
}

PositionRead read_resolved_record(PositionStore& store, std::uint32_t index,
                                  const ResolvedRead& input)
{
    if (index >= PositionStore::capacity)
        return {nullptr, ReadStatus::outside_supported_domain};
    auto& record = store.at(index);
    if (!input.source_available)
        return {&record, ReadStatus::source_unavailable};

    if (index <= 64 && (!input.slot_flag || input.slot_value <= 0)) {
        const bool enabled = input.options.select_alternate != 0
            ? input.options.alternate_enabled : input.options.primary_enabled;
        if (!enabled || !ordered_not_equal(0.0f, record.validity_marker)) {
            record.active = 0;
            return {&record, ReadStatus::cleared_by_policy};
        }
    }
    if (record.active == 0)
        return {&record, ReadStatus::inactive};
    const float age = input.now - record.updated_at;
    return {&record, below_or_unordered(age, 4.0f)
        ? ReadStatus::usable : ReadStatus::expired};
}

bool passes_item_consumer_gate(const PositionRead& read)
{
    return read.status == ReadStatus::usable && read.record != nullptr &&
           read.record->item_definition != 0;
}
} // namespace rage::offline

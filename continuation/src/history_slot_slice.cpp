#include "rage/history_slot_slice.hpp"

#include <array>
#include <cstring>

namespace rage::offline {
namespace {

struct ByteRange {
    std::size_t offset;
    std::size_t size;
};

constexpr std::array<ByteRange, 9> initial_zero_ranges{{
    {0x30, 0x10},
    {0x41, 0x04},
    {0x3d, 0x04},
    {0x48, 0x10},
    {0x7c, 0x10},
    {0x6c, 0x10},
    {0x5c, 0x10},
    {0x8c, 0x10},
    {0x9c, 0x10}
}};

constexpr std::array<std::uint8_t, 0x10> initial_vector{{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
}};

constexpr std::array<ByteRange, 6> final_zero_ranges{{
    {0x00, 0x10},
    {0x10, 0x10},
    {0x20, 0x10},
    {0x2c, 0x10},
    {0x74, 0x10},
    {0x64, 0x10}
}};

static_assert(sizeof(std::uint32_t) == 4);

void initialize_record(std::uint8_t* record) noexcept
{
    for (const ByteRange range : initial_zero_ranges)
        std::memset(record + range.offset, 0, range.size);
    std::memcpy(record + 0xac, initial_vector.data(), initial_vector.size());
    for (const ByteRange range : final_zero_ranges)
        std::memset(record + range.offset, 0, range.size);
}

} // namespace

HistorySlotResolution resolve_existing_history_slot(HistorySlotArrayView& view) noexcept
{
    const std::uint32_t next_count = view.field_10 + std::uint32_t{1};
    if (view.field_08 <= next_count)
        return {HistorySlotStatus::growth_call_required_unresolved, 0, {nullptr, 0}};

    const std::uint32_t mask = view.field_08 - std::uint32_t{1};
    const std::uint32_t masked_index = view.field_0c & mask;
    view.field_0c = masked_index;

    const std::uint32_t cursor_base = masked_index == 0 ? view.field_08 : masked_index;
    const std::uint32_t cursor = cursor_base - std::uint32_t{1};
    const std::uint32_t index = cursor & mask;

    if (view.entries == nullptr || static_cast<std::size_t>(index) >= view.entries_size)
        return {HistorySlotStatus::invalid_view, index, {nullptr, 0}};

    const HistoryDestinationBytes record = view.entries[index];
    if (record.data == nullptr)
        return {HistorySlotStatus::allocation_required_unresolved, index, {nullptr, 0}};
    if (record.size < history_slot_record_bytes ||
        (reinterpret_cast<std::uintptr_t>(record.data) & std::uintptr_t{0x0f}) != 0)
        return {HistorySlotStatus::invalid_view, index, {nullptr, 0}};

    initialize_record(record.data);
    view.field_0c = cursor;
    view.field_10 = next_count;
    return {HistorySlotStatus::resolved_existing_slot, index, record};
}

HistorySlotConsumerStatus copy_history_slot_record(
    std::optional<HistoryPostCallSlotObservation> post_call_slot,
    HistorySourceBytes source,
    std::optional<std::uint32_t> local_word_observation) noexcept
{
    if (!post_call_slot.has_value())
        return HistorySlotConsumerStatus::slot_unavailable;
    if (!local_word_observation.has_value())
        return HistorySlotConsumerStatus::observation_unavailable;
    if (copy_history_store_prefix(source, post_call_slot->record, *local_word_observation) !=
        HistorySliceStatus::copied)
        return HistorySlotConsumerStatus::invalid_view;
    return HistorySlotConsumerStatus::copied;
}

} // namespace rage::offline

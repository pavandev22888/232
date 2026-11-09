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

HistorySlotResolution initialize_selected_slot(HistorySlotArrayView& view) noexcept
{
    const std::uint32_t next_count = view.field_10 + std::uint32_t{1};
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

bool overlaps(const std::uint8_t* first, std::size_t first_size,
              const std::uint8_t* second, std::size_t second_size) noexcept
{
    if (first_size == 0 || second_size == 0)
        return false;
    const auto a = reinterpret_cast<std::uintptr_t>(first);
    const auto b = reinterpret_cast<std::uintptr_t>(second);
    return a <= b ? b - a < first_size : a - b < second_size;
}

} // namespace

HistorySlotResolution resolve_existing_history_slot(HistorySlotArrayView& view) noexcept
{
    if (view.field_08 <= view.field_10 + std::uint32_t{1})
        return {HistorySlotStatus::growth_call_required_unresolved, 0, {nullptr, 0}};
    return initialize_selected_slot(view);
}

HistorySlotResolution resume_history_slot_after_growth(HistorySlotArrayView& view) noexcept
{
    return initialize_selected_slot(view);
}

HistoryGrowthAllocationRequest plan_history_slot_growth(std::uint32_t field_08) noexcept
{
    if (field_08 > 0x11037u)
        return {HistoryGrowthRequestStatus::invalid_observation, 0, 0, false};
    std::uint32_t capacity = field_08 + (field_08 < 1u ? 1u : 0u);
    while (capacity < 8u || capacity - field_08 < 1u) {
        if (0x11037u - capacity < capacity)
            return {HistoryGrowthRequestStatus::length_failure_call_required_unresolved,
                    capacity, 0, false};
        capacity += capacity;
    }
    const bool aligned = capacity >= 0x400u;
    return {HistoryGrowthRequestStatus::allocation_call_required_unresolved,
            capacity, capacity * 4u + (aligned ? 0x23u : 0u), aligned};
}

HistoryGrowthCopyResult populate_history_growth_table(
    HistoryGrowthCopyObservation observation) noexcept
{
    HistoryGrowthCopyResult result{
        HistoryGrowthCopyStatus::invalid_observation, 0, {nullptr, 0}, 0, {}};
    const std::uint32_t target = observation.target_capacity;
    const std::uint32_t old = observation.field_08;
    const std::uint32_t cursor = observation.cursor;
    if (target < 8u || target > 0x11037u || old > target || cursor > old)
        return result;

    const bool aligned = observation.allocation.aligned_table;
    const std::size_t table_bytes = static_cast<std::size_t>(target) * 4u;
    const std::size_t allocation_bytes = table_bytes + (aligned ? 0x23u : 0u);
    const std::size_t old_bytes = static_cast<std::size_t>(old) * 4u;
    const auto& allocation = observation.allocation;
    if (allocation.eax_address == 0 || allocation.storage.data == nullptr ||
        allocation.storage.size < allocation_bytes ||
        std::uint64_t{allocation.eax_address} + allocation_bytes > 0x100000000ull ||
        (old_bytes != 0 && observation.old_table.data == nullptr) ||
        observation.old_table.size < old_bytes ||
        overlaps(observation.old_table.data, old_bytes,
                 allocation.storage.data, allocation_bytes))
        return result;

    const std::uint32_t table_address = aligned
        ? (allocation.eax_address + 0x23u) & ~std::uint32_t{0x1f}
        : allocation.eax_address;
    const std::size_t offset = table_address - allocation.eax_address;
    std::uint8_t* const table = allocation.storage.data + offset;
    if (aligned) {
        for (unsigned i = 0; i < 4; ++i)
            allocation.storage.data[offset - 4u + i] =
                static_cast<std::uint8_t>(allocation.eax_address >> (8u * i));
    }

    const std::uint32_t delta = target - old;
    auto& operations = result.operations;
    operations[0] = {HistoryGrowthOperationKind::copy_words, cursor, cursor, old - cursor};
    if (delta < cursor) {
        operations[1] = {HistoryGrowthOperationKind::copy_words, 0, old, delta};
        operations[2] = {HistoryGrowthOperationKind::copy_words, delta, 0, cursor - delta};
        operations[3] = {HistoryGrowthOperationKind::zero_words, 0, cursor - delta, delta};
    } else {
        operations[1] = {HistoryGrowthOperationKind::copy_words, 0, old, cursor};
        operations[2] = {HistoryGrowthOperationKind::zero_words, 0, old + cursor,
                         delta - cursor};
        operations[3] = {HistoryGrowthOperationKind::zero_words, 0, 0, cursor};
    }
    for (const auto& operation : operations) {
        if (operation.words == 0)
            continue;
        std::uint8_t* const destination = table + operation.destination_word * 4u;
        const std::size_t bytes = static_cast<std::size_t>(operation.words) * 4u;
        if (operation.kind == HistoryGrowthOperationKind::copy_words)
            std::memcpy(destination, observation.old_table.data + operation.source_word * 4u,
                        bytes);
        else
            std::memset(destination, 0, bytes);
    }
    result.status = HistoryGrowthCopyStatus::populated_release_unresolved;
    result.table_address = table_address;
    result.table = {table, table_bytes};
    result.capacity_delta = delta;
    return result;
}

HistoryGrowthReleaseRequest plan_history_growth_release(
    std::uint32_t field_04, std::uint32_t field_08,
    std::optional<std::uint32_t> word_before_table) noexcept
{
    if (field_04 == 0)
        return {HistoryGrowthReleaseStatus::no_call_required, 0, 0};
    std::uint32_t bytes = field_08 * std::uint32_t{4};
    if (bytes < 0x1000u)
        return {HistoryGrowthReleaseStatus::release_call_required_unresolved, field_04, bytes};
    if (!word_before_table.has_value())
        return {HistoryGrowthReleaseStatus::header_observation_required, 0, bytes};
    if (field_04 - std::uint32_t{4} - *word_before_table >= 0x20u)
        return {HistoryGrowthReleaseStatus::invalid_parameter_call_required_unresolved, 0, bytes};
    bytes += 0x23u;
    return {HistoryGrowthReleaseStatus::release_call_required_unresolved, *word_before_table, bytes};
}

void publish_history_growth_table(
    HistoryGrowthState& state, HistoryGrowthPostReleaseObservation observation) noexcept
{
    state.field_04 = observation.stack_table_address;
    state.field_08 += observation.ebx_capacity_delta;
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

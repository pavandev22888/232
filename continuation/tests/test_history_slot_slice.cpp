#include "rage/history_slot_slice.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <utility>

namespace {

unsigned assertions = 0;

#define CHECK(condition) do { \
    ++assertions; \
    if (!(condition)) { \
        std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; \
        return false; \
    } \
} while (0)

struct alignas(16) SlotBuffer {
    std::array<std::uint8_t, rage::offline::history_slot_record_bytes> bytes;
};

void fill_entries(std::array<SlotBuffer, 6>& buffers,
                  std::array<rage::offline::HistoryDestinationBytes, 6>& entries)
{
    for (std::size_t i = 0; i < buffers.size(); ++i) {
        buffers[i].bytes.fill(0xa5);
        entries[i] = {buffers[i].bytes.data(), buffers[i].bytes.size()};
    }
}

void put_word(std::uint8_t* bytes, std::size_t offset, std::uint32_t word)
{
    std::memcpy(bytes + offset, &word, sizeof(word));
}

bool selects_initializes_and_reaches_the_known_consumer()
{
    std::array<SlotBuffer, 6> buffers{};
    std::array<rage::offline::HistoryDestinationBytes, 6> entries{};
    fill_entries(buffers, entries);
    rage::offline::HistorySlotArrayView view{6, 7, 3, entries.data(), entries.size()};

    const auto slot = rage::offline::resolve_existing_history_slot(view);
    CHECK(slot.status == rage::offline::HistorySlotStatus::resolved_existing_slot);
    CHECK(slot.index == 4);
    CHECK(slot.record.data == buffers[4].bytes.data());
    CHECK(view.field_0c == 4);
    CHECK(view.field_10 == 4);

    std::array<std::uint8_t, rage::offline::history_slot_record_bytes> initialized{};
    initialized.fill(0xa5);
    const std::array<std::pair<std::size_t, std::size_t>, 9> first_zero_ranges{{
        {0x30, 0x10}, {0x41, 0x04}, {0x3d, 0x04}, {0x48, 0x10},
        {0x7c, 0x10}, {0x6c, 0x10}, {0x5c, 0x10}, {0x8c, 0x10}, {0x9c, 0x10}
    }};
    for (const auto& range : first_zero_ranges)
        std::memset(initialized.data() + range.first, 0, range.second);
    const std::array<std::uint8_t, 16> vector{{
        0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    }};
    std::memcpy(initialized.data() + 0xac, vector.data(), vector.size());
    const std::array<std::pair<std::size_t, std::size_t>, 6> final_zero_ranges{{
        {0, 0x10}, {0x10, 0x10}, {0x20, 0x10}, {0x2c, 0x10}, {0x74, 0x10}, {0x64, 0x10}
    }};
    for (const auto& range : final_zero_ranges)
        std::memset(initialized.data() + range.first, 0, range.second);
    CHECK(buffers[4].bytes == initialized);
    for (std::size_t i = 0; i < buffers.size(); ++i) {
        if (i != slot.index)
            CHECK(buffers[i].bytes[0] == 0xa5);
    }

    std::array<std::uint8_t, rage::offline::history_slice_source_bytes> source{};
    source.fill(0x37);
    put_word(source.data(), 0x10, 0x7fc12345u);
    put_word(source.data(), 0x38, 0xdeadbeefu);
    const auto before_missing_slot = buffers[1].bytes;
    CHECK(rage::offline::copy_history_slot_record(
              std::nullopt, {source.data(), source.size()}, std::uint32_t{0xcafebabeu}) ==
          rage::offline::HistorySlotConsumerStatus::slot_unavailable);
    CHECK(buffers[1].bytes == before_missing_slot);
    const auto before_missing_observation = buffers[1].bytes;
    CHECK(rage::offline::copy_history_slot_record(
              rage::offline::HistoryPostCallSlotObservation{
                  {buffers[1].bytes.data(), buffers[1].bytes.size()}},
              {source.data(), source.size()}, std::nullopt) ==
          rage::offline::HistorySlotConsumerStatus::observation_unavailable);
    CHECK(buffers[1].bytes == before_missing_observation);
    constexpr std::uint32_t local_word = 0xcafebabeu;
    CHECK(rage::offline::copy_history_slot_record(
              rage::offline::HistoryPostCallSlotObservation{
                  {buffers[1].bytes.data(), buffers[1].bytes.size()}},
              {source.data(), source.size()}, std::optional<std::uint32_t>{local_word}) ==
          rage::offline::HistorySlotConsumerStatus::copied);
    CHECK(buffers[1].bytes[0] == 0x45);
    CHECK(buffers[1].bytes[1] == 0x23);
    CHECK(buffers[1].bytes[2] == 0xc1);
    CHECK(buffers[1].bytes[3] == 0x7f);
    CHECK(buffers[1].bytes[0x80] == 0xef);
    CHECK(buffers[1].bytes[0x3c] == 0xbe);
    CHECK(buffers[1].bytes[0x3d] == 0xba);
    CHECK(buffers[1].bytes[0x3e] == 0xfe);
    CHECK(buffers[1].bytes[0x3f] == 0xca);
    CHECK(buffers[1].bytes[0xb0] == 0);
    CHECK(buffers[1].bytes[0xb4] == 0xa5);
    CHECK(buffers[1].bytes[0xbb] == 0xa5);
    CHECK(buffers[4].bytes == initialized);
    return true;
}

bool unresolved_and_invalid_views_stop_at_the_frontier()
{
    std::array<SlotBuffer, 6> buffers{};
    std::array<rage::offline::HistoryDestinationBytes, 6> entries{};
    fill_entries(buffers, entries);

    rage::offline::HistorySlotArrayView growth{6, 2, 5, entries.data(), entries.size()};
    const auto growth_result = rage::offline::resolve_existing_history_slot(growth);
    CHECK(growth_result.status == rage::offline::HistorySlotStatus::growth_call_required_unresolved);
    CHECK(growth.field_0c == 2);
    CHECK(growth.field_10 == 5);
    CHECK(buffers[0].bytes[0] == 0xa5);

    entries[5] = {nullptr, 0};
    rage::offline::HistorySlotArrayView absent{6, 10, 3, entries.data(), entries.size()};
    const auto absent_result = rage::offline::resolve_existing_history_slot(absent);
    CHECK(absent_result.status == rage::offline::HistorySlotStatus::allocation_required_unresolved);
    CHECK(absent_result.index == 5);
    CHECK(absent.field_0c == 0);
    CHECK(absent.field_10 == 3);

    rage::offline::HistorySlotArrayView out_of_bounds{6, 7, 3, entries.data(), 4};
    const auto bounds_result = rage::offline::resolve_existing_history_slot(out_of_bounds);
    CHECK(bounds_result.status == rage::offline::HistorySlotStatus::invalid_view);
    CHECK(bounds_result.index == 4);
    CHECK(out_of_bounds.field_0c == 5);
    CHECK(out_of_bounds.field_10 == 3);

    entries[4] = {buffers[4].bytes.data(), buffers[4].bytes.size() - 1};
    rage::offline::HistorySlotArrayView short_record{6, 7, 3, entries.data(), entries.size()};
    const auto short_result = rage::offline::resolve_existing_history_slot(short_record);
    CHECK(short_result.status == rage::offline::HistorySlotStatus::invalid_view);
    CHECK(buffers[4].bytes[0] == 0xa5);
    CHECK(short_record.field_10 == 3);

    rage::offline::HistorySlotArrayView missing_table{6, 7, 3, nullptr, entries.size()};
    CHECK(rage::offline::resolve_existing_history_slot(missing_table).status ==
          rage::offline::HistorySlotStatus::invalid_view);
    return true;
}

bool non_power_of_two_capacity_keeps_cursor_distinct_from_masked_index()
{
    std::array<SlotBuffer, 5> buffers{};
    for (auto& buffer : buffers)
        buffer.bytes.fill(0xa5);
    std::array<rage::offline::HistoryDestinationBytes, 5> entries{};
    for (std::size_t i = 0; i < buffers.size(); ++i)
        entries[i] = {buffers[i].bytes.data(), buffers[i].bytes.size()};

    rage::offline::HistorySlotArrayView view{5, 4, 0, entries.data(), entries.size()};
    const auto slot = rage::offline::resolve_existing_history_slot(view);
    CHECK(slot.status == rage::offline::HistorySlotStatus::resolved_existing_slot);
    CHECK(slot.index == 0);
    CHECK(slot.record.data == buffers[0].bytes.data());
    CHECK(view.field_0c == 3);
    CHECK(view.field_0c != slot.index);
    CHECK(view.field_10 == 1);
    return true;
}

bool unsigned_word_wrap_matches_the_decoded_inc()
{
    std::array<SlotBuffer, 1> buffers{};
    buffers[0].bytes.fill(0xa5);
    std::array<rage::offline::HistoryDestinationBytes, 1> entries{{
        {buffers[0].bytes.data(), buffers[0].bytes.size()}
    }};
    rage::offline::HistorySlotArrayView view{
        1, 0xffffffffu, std::numeric_limits<std::uint32_t>::max(), entries.data(), entries.size()
    };
    const auto slot = rage::offline::resolve_existing_history_slot(view);
    CHECK(slot.status == rage::offline::HistorySlotStatus::resolved_existing_slot);
    CHECK(slot.index == 0);
    CHECK(view.field_0c == 0);
    CHECK(view.field_10 == 0);
    return true;
}

} // namespace

int main()
{
    if (!selects_initializes_and_reaches_the_known_consumer() ||
        !unresolved_and_invalid_views_stop_at_the_frontier() ||
        !non_power_of_two_capacity_keeps_cursor_distinct_from_masked_index() ||
        !unsigned_word_wrap_matches_the_decoded_inc())
        return 1;
    std::cout << "PASS test_history_slot_slice assertions=" << assertions << '\n';
    return 0;
}

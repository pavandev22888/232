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

std::uint32_t get_word(const std::uint8_t* bytes, std::size_t word_index)
{
    const std::uint8_t* p = bytes + word_index * 4;
    return std::uint32_t{p[0]} | (std::uint32_t{p[1]} << 8u) |
           (std::uint32_t{p[2]} << 16u) | (std::uint32_t{p[3]} << 24u);
}

bool growth_request_retains_unsigned_limits_and_non_power_of_two_capacities()
{
    using namespace rage::offline;
    for (const auto& item : std::array<std::pair<std::uint32_t, std::uint32_t>, 9>{{
             {0, 8}, {1, 8}, {3, 12}, {5, 10}, {8, 16},
             {0x1ff, 0x3fe}, {0x200, 0x400}, {0x400, 0x800}, {0x881b, 0x11036}}}) {
        const auto request = plan_history_slot_growth(item.first);
        CHECK(request.status == HistoryGrowthRequestStatus::allocation_call_required_unresolved);
        CHECK(request.target_capacity == item.second);
        CHECK(request.aligned_table == (item.second >= 0x400));
        CHECK(request.bytes == item.second * 4u + (request.aligned_table ? 0x23u : 0u));
    }
    CHECK(plan_history_slot_growth(0x881c).status ==
          HistoryGrowthRequestStatus::length_failure_call_required_unresolved);
    CHECK(plan_history_slot_growth(0x11037).status ==
          HistoryGrowthRequestStatus::length_failure_call_required_unresolved);
    CHECK(plan_history_slot_growth(0x11038).status ==
          HistoryGrowthRequestStatus::invalid_observation);
    CHECK(plan_history_slot_growth(0x80000000).status ==
          HistoryGrowthRequestStatus::invalid_observation);
    return true;
}

bool growth_copies_and_zeros_in_both_source_orders()
{
    using namespace rage::offline;
    std::array<std::uint8_t, 32> old{};
    for (std::size_t i = 0; i < 8; ++i)
        put_word(old.data(), i * 4, static_cast<std::uint32_t>(0x12340001u + i));
    const auto old_before = old;
    std::array<std::uint8_t, 50> fresh{};
    fresh.fill(0xa5);
    auto result = populate_history_growth_table({
        12, 3, 5, {old.data(), old.size()}, {0x2000, false, {fresh.data() + 1, 48}}});
    CHECK(result.status == HistoryGrowthCopyStatus::populated_release_unresolved);
    CHECK(result.table_address == 0x2000);
    CHECK(result.capacity_delta == 7);
    CHECK(fresh.front() == 0xa5 && fresh.back() == 0xa5);
    const std::array<std::uint32_t, 12> expected{{
        0, 0, 0, 0x12340004, 0x12340005, 0x12340001, 0x12340002, 0x12340003, 0, 0, 0, 0}};
    for (std::size_t i = 0; i < expected.size(); ++i)
        CHECK(get_word(result.table.data, i) == expected[i]);
    const std::array<HistoryGrowthOperation, 4> wide{{
        {HistoryGrowthOperationKind::copy_words, 3, 3, 2},
        {HistoryGrowthOperationKind::copy_words, 0, 5, 3},
        {HistoryGrowthOperationKind::zero_words, 0, 8, 4},
        {HistoryGrowthOperationKind::zero_words, 0, 0, 3}}};
    for (std::size_t i = 0; i < wide.size(); ++i) {
        CHECK(result.operations[i].kind == wide[i].kind);
        CHECK(result.operations[i].source_word == wide[i].source_word);
        CHECK(result.operations[i].destination_word == wide[i].destination_word);
        CHECK(result.operations[i].words == wide[i].words);
    }

    fresh.fill(0xa5);
    result = populate_history_growth_table({
        10, 5, 8, {old.data(), old.size()}, {0x3000, false, {fresh.data() + 1, 40}}});
    CHECK(result.status == HistoryGrowthCopyStatus::populated_release_unresolved);
    CHECK(result.capacity_delta == 2);
    const std::array<std::uint32_t, 10> narrow_expected{{
        0x12340003, 0x12340004, 0x12340005, 0, 0,
        0x12340006, 0x12340007, 0x12340008, 0x12340001, 0x12340002}};
    for (std::size_t i = 0; i < narrow_expected.size(); ++i)
        CHECK(get_word(result.table.data, i) == narrow_expected[i]);
    const std::array<HistoryGrowthOperation, 4> narrow{{
        {HistoryGrowthOperationKind::copy_words, 5, 5, 3},
        {HistoryGrowthOperationKind::copy_words, 0, 8, 2},
        {HistoryGrowthOperationKind::copy_words, 2, 0, 3},
        {HistoryGrowthOperationKind::zero_words, 0, 3, 2}}};
    for (std::size_t i = 0; i < narrow.size(); ++i) {
        CHECK(result.operations[i].kind == narrow[i].kind);
        CHECK(result.operations[i].source_word == narrow[i].source_word);
        CHECK(result.operations[i].destination_word == narrow[i].destination_word);
        CHECK(result.operations[i].words == narrow[i].words);
    }
    CHECK(old == old_before);
    CHECK(fresh.front() == 0xa5 && fresh[41] == 0xa5);

    result = populate_history_growth_table({
        10, 5, 5, {old.data(), old.size()}, {0x3000, false, {fresh.data() + 1, 40}}});
    CHECK(result.status == HistoryGrowthCopyStatus::populated_release_unresolved);
    CHECK(result.operations[0].words == 0);
    CHECK(result.operations[2].words == 0);
    for (std::size_t i = 0; i < 5; ++i) {
        CHECK(get_word(result.table.data, i) == 0);
        CHECK(get_word(result.table.data, i + 5) == 0x12340001u + i);
    }
    return true;
}

bool allocation_observations_control_header_and_do_not_allocate()
{
    using namespace rage::offline;
    std::array<std::uint8_t, 0x1000 + 0x23 + 2> bytes{};
    bytes.fill(0xa5);
    const auto request = plan_history_slot_growth(0x200);
    const auto result = populate_history_growth_table({
        request.target_capacity, 0, 0, {nullptr, 0},
        {0x1001, request.aligned_table, {bytes.data() + 1, request.bytes}}});
    CHECK(result.status == HistoryGrowthCopyStatus::populated_release_unresolved);
    CHECK(result.table_address == 0x1020);
    CHECK(result.table.data == bytes.data() + 32);
    CHECK(result.table.size == 0x1000);
    CHECK(bytes[28] == 0x01 && bytes[29] == 0x10 && bytes[30] == 0 && bytes[31] == 0);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i >= 32 && i < 32 + 0x1000)
            CHECK(bytes[i] == 0);
        else if (i < 28 || i >= 32)
            CHECK(bytes[i] == 0xa5);
    }

    std::array<std::uint8_t, 67> small{};
    small.fill(0xa5);
    const auto changed_ebx = populate_history_growth_table({
        8, 0, 0, {nullptr, 0}, {0x401c, true, {small.data(), small.size()}}});
    CHECK(changed_ebx.status == HistoryGrowthCopyStatus::populated_release_unresolved);
    CHECK(changed_ebx.table_address == 0x4020);
    CHECK(get_word(small.data(), 0) == 0x401c);
    CHECK(changed_ebx.table.data == small.data() + 4);
    CHECK(changed_ebx.table.size == 32);
    return true;
}

bool invalid_growth_observations_do_not_write()
{
    using namespace rage::offline;
    std::array<std::uint8_t, 32> old{};
    std::array<std::uint8_t, 64> fresh{};
    fresh.fill(0xa5);
    const auto before = fresh;
    HistoryGrowthCopyObservation valid{
        8, 0, 0, {nullptr, 0}, {0x2000, false, {fresh.data(), fresh.size()}}};
    auto bad = valid;
    bad.cursor = 1;
    CHECK(populate_history_growth_table(bad).status == HistoryGrowthCopyStatus::invalid_observation);
    bad = valid; bad.field_08 = 9;
    CHECK(populate_history_growth_table(bad).status == HistoryGrowthCopyStatus::invalid_observation);
    bad = valid; bad.target_capacity = 0x11038;
    CHECK(populate_history_growth_table(bad).status == HistoryGrowthCopyStatus::invalid_observation);
    bad = valid; bad.allocation.storage.size = 31;
    CHECK(populate_history_growth_table(bad).status == HistoryGrowthCopyStatus::invalid_observation);
    bad = valid; bad.allocation.eax_address = 0xfffffff0u;
    CHECK(populate_history_growth_table(bad).status == HistoryGrowthCopyStatus::invalid_observation);
    bad = valid; bad.allocation.eax_address = 0;
    CHECK(populate_history_growth_table(bad).status == HistoryGrowthCopyStatus::invalid_observation);
    bad = valid; bad.field_08 = 1;
    CHECK(populate_history_growth_table(bad).status == HistoryGrowthCopyStatus::invalid_observation);
    bad.old_table = {old.data(), 3};
    CHECK(populate_history_growth_table(bad).status == HistoryGrowthCopyStatus::invalid_observation);
    bad.old_table = {fresh.data() + 4, 4};
    CHECK(populate_history_growth_table(bad).status == HistoryGrowthCopyStatus::invalid_observation);
    bad = valid; bad.allocation.storage.data = nullptr;
    CHECK(populate_history_growth_table(bad).status == HistoryGrowthCopyStatus::invalid_observation);
    CHECK(fresh == before);
    return true;
}

bool release_and_publication_use_fresh_independent_observations()
{
    using namespace rage::offline;
    CHECK(plan_history_growth_release(0, 0xffffffffu, std::nullopt).status ==
          HistoryGrowthReleaseStatus::no_call_required);
    const auto small = plan_history_growth_release(0x3000, 0x3ff, std::nullopt);
    CHECK(small.status == HistoryGrowthReleaseStatus::release_call_required_unresolved);
    CHECK(small.address == 0x3000 && small.bytes == 0xffc);
    CHECK(plan_history_growth_release(0x3020, 0x400, std::nullopt).status ==
          HistoryGrowthReleaseStatus::header_observation_required);
    const auto large = plan_history_growth_release(0x3020, 0x400, 0x3001);
    CHECK(large.status == HistoryGrowthReleaseStatus::release_call_required_unresolved);
    CHECK(large.address == 0x3001 && large.bytes == 0x1023);
    CHECK(plan_history_growth_release(0x3020, 0x400, 0x301c).status ==
          HistoryGrowthReleaseStatus::release_call_required_unresolved);
    CHECK(plan_history_growth_release(0x3020, 0x400, 0x2ffd).status ==
          HistoryGrowthReleaseStatus::release_call_required_unresolved);
    CHECK(plan_history_growth_release(0x3020, 0x400, 0x2ffc).status ==
          HistoryGrowthReleaseStatus::invalid_parameter_call_required_unresolved);
    CHECK(plan_history_growth_release(0x3020, 0x400, 0x3020).status ==
          HistoryGrowthReleaseStatus::invalid_parameter_call_required_unresolved);
    const auto wrapped_size = plan_history_growth_release(0x4000, 0x40000001, std::nullopt);
    CHECK(wrapped_size.status == HistoryGrowthReleaseStatus::release_call_required_unresolved);
    CHECK(wrapped_size.bytes == 4);
    const auto wrapped_large = plan_history_growth_release(0x4020, 0x3fffffff, 0x4000);
    CHECK(wrapped_large.status == HistoryGrowthReleaseStatus::release_call_required_unresolved);
    CHECK(wrapped_large.bytes == 0x1f);

    HistoryGrowthState state{0xdead, 0xfffffff8u, 0x33, 7};
    publish_history_growth_table(state, {0x5020, 10});
    CHECK(state.field_04 == 0x5020);
    CHECK(state.field_08 == 2);
    CHECK(state.field_0c == 0x33 && state.field_10 == 7);
    return true;
}

bool growth_table_reaches_resolver_but_does_not_invent_null_slot_allocation()
{
    using namespace rage::offline;
    const auto request = plan_history_slot_growth(5);
    std::array<std::uint8_t, 20> old{};
    for (std::size_t i = 0; i < 5; ++i)
        put_word(old.data(), i * 4, static_cast<std::uint32_t>(0x1000 + i * 0x100));
    std::array<std::uint8_t, 40> fresh{};
    const auto table = populate_history_growth_table({
        request.target_capacity, 4, 5, {old.data(), old.size()},
        {0x2000, false, {fresh.data(), fresh.size()}}});
    CHECK(table.status == HistoryGrowthCopyStatus::populated_release_unresolved);
    CHECK(get_word(table.table.data, 9) == 0);
    CHECK(plan_history_growth_release(0x9000, 5, std::nullopt).status ==
          HistoryGrowthReleaseStatus::release_call_required_unresolved);
    HistoryGrowthState observed_after_release{0x9000, 5, 4, 4};
    publish_history_growth_table(observed_after_release, {table.table_address, table.capacity_delta});
    CHECK(observed_after_release.field_08 == 10);
    CHECK(observed_after_release.field_04 == 0x2000);

    std::array<HistoryDestinationBytes, 10> entries{};
    HistorySlotArrayView observed_after_growth{10, 4, 9, entries.data(), entries.size()};
    const auto result = resume_history_slot_after_growth(observed_after_growth);
    CHECK(result.status == HistorySlotStatus::allocation_required_unresolved);
    CHECK(result.index == 9);
    CHECK(observed_after_growth.field_0c == 0);
    CHECK(observed_after_growth.field_10 == 9);

    alignas(16) std::array<std::uint8_t, history_slot_record_bytes> record{};
    record.fill(0xa5);
    entries[0] = {record.data(), record.size()};
    HistorySlotArrayView distinct_post_growth{5, 4, 9, entries.data(), entries.size()};
    const auto existing = resume_history_slot_after_growth(distinct_post_growth);
    CHECK(existing.status == HistorySlotStatus::resolved_existing_slot);
    CHECK(existing.index == 0);
    CHECK(distinct_post_growth.field_0c == 3);
    CHECK(distinct_post_growth.field_10 == 10);
    CHECK(record[0] == 0 && record[0xb4] == 0xff);
    return true;
}

} // namespace

int main()
{
    if (!selects_initializes_and_reaches_the_known_consumer() ||
        !unresolved_and_invalid_views_stop_at_the_frontier() ||
        !non_power_of_two_capacity_keeps_cursor_distinct_from_masked_index() ||
        !unsigned_word_wrap_matches_the_decoded_inc() ||
        !growth_request_retains_unsigned_limits_and_non_power_of_two_capacities() ||
        !growth_copies_and_zeros_in_both_source_orders() ||
        !allocation_observations_control_header_and_do_not_allocate() ||
        !invalid_growth_observations_do_not_write() ||
        !release_and_publication_use_fresh_independent_observations() ||
        !growth_table_reaches_resolver_but_does_not_invent_null_slot_allocation())
        return 1;
    std::cout << "PASS test_history_slot_slice assertions=" << assertions << '\n';
    return 0;
}

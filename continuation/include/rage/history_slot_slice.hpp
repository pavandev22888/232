#ifndef RAGE_HISTORY_SLOT_SLICE_HPP
#define RAGE_HISTORY_SLOT_SLICE_HPP

#include "rage/history_slice.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rage::offline {

inline constexpr std::size_t history_slot_record_bytes = 0xbc;

struct HistorySlotArrayView {
    std::uint32_t field_08;
    std::uint32_t field_0c;
    std::uint32_t field_10;
    HistoryDestinationBytes* entries;
    std::size_t entries_size;
};

enum class HistorySlotStatus {
    resolved_existing_slot,
    growth_call_required_unresolved,
    allocation_required_unresolved,
    invalid_view
};

struct HistorySlotResolution {
    HistorySlotStatus status;
    std::uint32_t index;
    HistoryDestinationBytes record;
};

// Models only the no-growth, already-present-slot branch of 0x413F88F0.
// field_08/0c/10 are raw DWORD observations at those ECX-relative offsets.
// entries is an offline mapping of the 32-bit pointer table, not its ABI layout.
// The view, entries array, and selected record need stable, pairwise non-aliasing
// storage; modeled +0x0c/+0x10 fields are updated. Allocator paths stay unresolved.
HistorySlotResolution resolve_existing_history_slot(HistorySlotArrayView& view) noexcept;

// Starts at 0x413F8909 with a separately observed post-growth ESI mapping.
// No pre-call view, capacity, count, cursor, or object identity is carried across.
HistorySlotResolution resume_history_slot_after_growth(HistorySlotArrayView& view) noexcept;

enum class HistoryGrowthRequestStatus {
    allocation_call_required_unresolved,
    length_failure_call_required_unresolved,
    invalid_observation
};

struct HistoryGrowthAllocationRequest {
    HistoryGrowthRequestStatus status;
    std::uint32_t target_capacity;
    std::uint32_t bytes;
    bool aligned_table;
};

// 0x413F3890's request=1 prefix, bounded to observed capacity <= 0x11037.
// This domain bound is offline safety, not an original entry guard.
HistoryGrowthAllocationRequest plan_history_slot_growth(std::uint32_t field_08) noexcept;

struct HistoryGrowthAllocationObservation {
    std::uint32_t eax_address;
    bool aligned_table; // branch selected before the opaque allocation call
    HistoryDestinationBytes storage;
};

struct HistoryGrowthCopyObservation {
    std::uint32_t target_capacity; // post-allocation EBX
    std::uint32_t cursor;          // post-allocation EBP
    std::uint32_t field_08;        // reloaded through post-allocation EDI
    HistorySourceBytes old_table;
    HistoryGrowthAllocationObservation allocation;
};

enum class HistoryGrowthOperationKind { copy_words, zero_words };

struct HistoryGrowthOperation {
    HistoryGrowthOperationKind kind;
    std::uint32_t source_word;
    std::uint32_t destination_word;
    std::uint32_t words;
};

enum class HistoryGrowthCopyStatus { populated_release_unresolved, invalid_observation };

struct HistoryGrowthCopyResult {
    HistoryGrowthCopyStatus status;
    std::uint32_t table_address;
    HistoryDestinationBytes table;
    std::uint32_t capacity_delta;
    std::array<HistoryGrowthOperation, 4> operations;
};

// Raw little-endian DWORD pointer tokens, never host pointer descriptors.
// Stable, non-aliasing descriptors and disjoint old/allocation byte storage
// are required. Allocator/lifetime behavior is not supplied by this adapter.
HistoryGrowthCopyResult populate_history_growth_table(
    HistoryGrowthCopyObservation observation) noexcept;

enum class HistoryGrowthReleaseStatus {
    no_call_required,
    release_call_required_unresolved,
    header_observation_required,
    invalid_parameter_call_required_unresolved
};

struct HistoryGrowthReleaseRequest {
    HistoryGrowthReleaseStatus status;
    std::uint32_t address;
    std::uint32_t bytes;
};

// Fresh observations at 0x413F39DC, not presumed allocator/old-table identity.
HistoryGrowthReleaseRequest plan_history_growth_release(
    std::uint32_t field_04, std::uint32_t field_08,
    std::optional<std::uint32_t> word_before_table) noexcept;

struct HistoryGrowthState {
    std::uint32_t field_04;
    std::uint32_t field_08;
    std::uint32_t field_0c;
    std::uint32_t field_10;
};

struct HistoryGrowthPostReleaseObservation {
    std::uint32_t stack_table_address;
    std::uint32_t ebx_capacity_delta;
};

// Only the two stores at 0x413F3A10/13; state maps post-call EDI independently.
void publish_history_growth_table(
    HistoryGrowthState& state, HistoryGrowthPostReleaseObservation observation) noexcept;

struct HistoryPostCallSlotObservation {
    HistoryDestinationBytes record;
};

enum class HistorySlotConsumerStatus {
    copied,
    slot_unavailable,
    observation_unavailable,
    invalid_view
};

// Requires a distinct explicit slot observation made after opaque 0x41B70930;
// it never assumes the pre-call resolution still identifies the consumer slot.
HistorySlotConsumerStatus copy_history_slot_record(
    std::optional<HistoryPostCallSlotObservation> post_call_slot,
    HistorySourceBytes source,
    std::optional<std::uint32_t> local_word_observation) noexcept;

} // namespace rage::offline
#endif

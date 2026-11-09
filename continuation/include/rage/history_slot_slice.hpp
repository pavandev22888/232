#ifndef RAGE_HISTORY_SLOT_SLICE_HPP
#define RAGE_HISTORY_SLOT_SLICE_HPP

#include "rage/history_slice.hpp"

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

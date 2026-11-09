#ifndef RAGE_HISTORY_SLICE_HPP
#define RAGE_HISTORY_SLICE_HPP

#include <cstddef>
#include <cstdint>

namespace rage::offline {

inline constexpr std::size_t history_slice_source_bytes = 0x9c;
inline constexpr std::size_t history_slice_destination_bytes = 0xb4;

struct HistorySourceBytes {
    const std::uint8_t* data;
    std::size_t size;
};

struct HistoryDestinationBytes {
    std::uint8_t* data;
    std::size_t size;
};

enum class HistorySliceStatus {
    copied,
    invalid_view
};

// Models the local 32-bit store prefix after source and slot pointers are resolved.
// The views are independent opaque pointers; selection and identity are not inferred.
// Short/null views are rejected by this offline adapter, not by a recovered branch.
HistorySliceStatus copy_history_store_prefix(
    HistorySourceBytes source,
    HistoryDestinationBytes destination,
    std::uint32_t local_word_at_ebp_minus_38) noexcept;

} // namespace rage::offline
#endif

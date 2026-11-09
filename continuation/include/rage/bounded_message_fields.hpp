#ifndef RAGE_BOUNDED_MESSAGE_FIELDS_HPP
#define RAGE_BOUNDED_MESSAGE_FIELDS_HPP

#include "rage/position_records.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace rage::offline {

// Logical bits only; this is not the original reader's padded DWORD-load ABI.
struct MessageBitCursor {
    const std::uint8_t* data;
    std::size_t size_bytes;
    std::int32_t bit_limit;
    std::int32_t position;
    bool overflow;
};

struct MessageFieldInputs {
    std::uint32_t expected_word;
    std::uint32_t reference_word;
    std::int32_t tolerance;
};

struct BoundedMessageFields {
    std::uint32_t first_word{};
    std::uint32_t zero_based_index{};
    std::uint32_t numeric_tag{};
    Vec3 position{};
    std::uint32_t extra_bits{};
};

enum class MessageFieldEffect {
    ready_for_consumer,
    first_word_mismatch,
    tag_outside_window,
    read_overflow,
    invalid_bit_view
};

struct BoundedMessageResult {
    MessageFieldEffect effect;
    BoundedMessageFields fields;
};

// Conditional slice 415128F8..41512AB9; ready selects the call at 41512BE8,
// not downstream acceptance or a complete packet. Unread outputs stay zero.
BoundedMessageResult decode_bounded_message_fields(
    MessageBitCursor& reader, const MessageFieldInputs& inputs);

struct MessageReceiverEpochObservation {
    std::uint32_t global_word;
    std::uint32_t thread_word;
};

// Values observed at the named reads/calls, not host pointers or providers.
struct MessageReceiverPrefixObservations {
    std::optional<std::uint32_t> dispatch_return_eax; // 4150ABEB
    std::optional<std::uint32_t> guard_offset_word; // [[4223FE60]]
    std::optional<MessageReceiverEpochObservation> guard_epoch; // 4250775C / TLS+8
    std::optional<std::uint32_t> guard_word; // [[4250770C]+guard_offset_word]
    std::optional<std::uint32_t> source_return_eax; // 41B6C670
    std::optional<MessageReceiverEpochObservation> slot_epoch; // 4250065C / TLS+8
    std::optional<std::uint32_t> slot_base_word; // [424FDD9C]
    std::optional<std::uint8_t> slot_byte; // computed slot_address
};

// Selected direct frame stores; provider side effects are not reconstructed.
struct MessageReceiverPrefixState {
    std::uint32_t scratch_1c;
    std::uint32_t exception_state_128;
};

enum class MessageReceiverPrefixEdge {
    index_above_64,
    dispatch_return_unobserved,
    index_matches_dispatch,
    guard_offset_unobserved,
    guard_epoch_unobserved,
    guard_epoch_side_exit,
    guard_word_unobserved,
    guard_word_nonzero,
    source_return_unobserved,
    source_zero,
    slot_epoch_unobserved,
    slot_epoch_side_exit,
    slot_base_unobserved,
    slot_base_zero_path,
    slot_byte_unobserved,
    slot_byte_zero,
    next_epoch_gate
};

struct MessageReceiverPrefixResult {
    MessageReceiverPrefixEdge edge;
    std::optional<std::uint32_t> slot_address;
};

// 4150ABEE..4150AC88 and the nonzero-base edge 4150AFA7..4150AFB4.
// Entry already returned from the unresolved dispatch. index_word is actual
// EDI there; parser index+1 is conditional on intervening register preservation.
// Stops at side exits/next gate; never invokes providers or updates N.
MessageReceiverPrefixResult evaluate_message_receiver_prefix(
    std::uint32_t index_word,
    const MessageReceiverPrefixObservations& observations,
    MessageReceiverPrefixState& state);

} // namespace rage::offline

#endif

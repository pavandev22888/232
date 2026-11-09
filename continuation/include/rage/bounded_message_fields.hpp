#ifndef RAGE_BOUNDED_MESSAGE_FIELDS_HPP
#define RAGE_BOUNDED_MESSAGE_FIELDS_HPP

#include "rage/position_records.hpp"

#include <cstddef>
#include <cstdint>

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

} // namespace rage::offline

#endif

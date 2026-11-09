#include "rage/bounded_message_fields.hpp"

#include <limits>

namespace rage::offline {
namespace {

bool valid_view(const MessageBitCursor& reader)
{
    if (reader.bit_limit < 0 ||
        reader.bit_limit > std::numeric_limits<std::int32_t>::max() - 32 ||
        reader.position < 0 || reader.position > reader.bit_limit) {
        return false;
    }
    const auto required_bytes = (static_cast<std::size_t>(reader.bit_limit) + 7) / 8;
    return required_bytes <= reader.size_bytes &&
           (reader.bit_limit == 0 || reader.data != nullptr);
}

std::uint32_t read_bit(MessageBitCursor& reader)
{
    if (reader.position >= reader.bit_limit) {
        reader.overflow = true;
        return 0;
    }
    const auto position = static_cast<std::uint32_t>(reader.position++);
    return (reader.data[position / 8] >> (position % 8)) & 1u;
}

std::uint32_t read_bits(MessageBitCursor& reader, std::int32_t count)
{
    if (count > reader.bit_limit - reader.position) {
        reader.position = reader.bit_limit;
        reader.overflow = true;
        return 0;
    }
    std::uint32_t value = 0;
    for (std::int32_t i = 0; i < count; ++i) {
        value |= read_bit(reader) << i;
    }
    return value;
}

float read_coordinate(MessageBitCursor& reader)
{
    const auto has_integer = read_bit(reader);
    const auto has_fraction = read_bit(reader);
    if ((has_integer | has_fraction) == 0) {
        return 0.0f;
    }
    const auto negative = read_bit(reader);
    const auto integer = has_integer ? read_bits(reader, 14) + 1u : 0u;
    const auto fraction = has_fraction ? read_bits(reader, 5) : 0u;
    const auto value = static_cast<float>(static_cast<double>(integer) +
                                         static_cast<double>(fraction) * 0.03125);
    return negative ? -value : value;
}

std::int64_t signed_word(std::uint32_t word)
{
    return word <= UINT32_C(0x7fffffff) ? static_cast<std::int64_t>(word) :
           static_cast<std::int64_t>(word) - INT64_C(0x100000000);
}

bool passes_tag_gate(std::uint32_t tag, const MessageFieldInputs& inputs)
{
    const auto difference = tag - inputs.reference_word;
    const auto negated = 0u - difference;
    // NEG/CMOVS is not mathematical abs: 0x80000000 remains signed-negative.
    const auto selected = signed_word(negated) < 0 ? difference : negated;
    return signed_word(selected) <= inputs.tolerance;
}

} // namespace

BoundedMessageResult decode_bounded_message_fields(
    MessageBitCursor& reader, const MessageFieldInputs& inputs)
{
    BoundedMessageResult result{MessageFieldEffect::invalid_bit_view, {}};
    if (!valid_view(reader)) {
        return result;
    }
    auto& fields = result.fields;
    fields.first_word = read_bits(reader, 32);
    if (fields.first_word != inputs.expected_word) {
        result.effect = MessageFieldEffect::first_word_mismatch;
        return result;
    }
    fields.zero_based_index = read_bits(reader, 7);
    fields.numeric_tag = read_bits(reader, 32);
    if (!passes_tag_gate(fields.numeric_tag, inputs)) {
        result.effect = MessageFieldEffect::tag_outside_window;
        return result;
    }
    fields.position.z = read_coordinate(reader);
    fields.position.y = read_coordinate(reader);
    fields.position.x = read_coordinate(reader);
    fields.extra_bits = read_bits(reader, 7);
    result.effect = reader.overflow ? MessageFieldEffect::read_overflow :
                                    MessageFieldEffect::ready_for_consumer;
    return result;
}

} // namespace rage::offline

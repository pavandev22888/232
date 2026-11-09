#include "rage/shot_result_slice.hpp"

namespace rage::offline {
namespace {

std::uint32_t read_word(const std::uint8_t* data) noexcept
{
    return std::uint32_t{data[0]} | (std::uint32_t{data[1]} << 8) |
           (std::uint32_t{data[2]} << 16) | (std::uint32_t{data[3]} << 24);
}

void write_word(std::uint8_t* data, std::uint32_t value) noexcept
{
    data[0] = static_cast<std::uint8_t>(value);
    data[1] = static_cast<std::uint8_t>(value >> 8);
    data[2] = static_cast<std::uint8_t>(value >> 16);
    data[3] = static_cast<std::uint8_t>(value >> 24);
}

bool valid(ShotResultFrameView frame) noexcept
{
    return frame.data != nullptr && frame.size >= shot_result_frame_bytes;
}

} // namespace

ShotResultIndex shot_result_index(std::uint32_t index_word) noexcept
{
    const std::int64_t signed_index = index_word < 0x80000000u
        ? static_cast<std::int64_t>(index_word)
        : static_cast<std::int64_t>(index_word) - 0x100000000LL;
    const auto quotient = static_cast<std::int32_t>(signed_index / 150);
    const auto remainder = static_cast<std::int32_t>(signed_index % 150);
    const std::uint32_t offset = static_cast<std::uint32_t>(remainder) * 0x98u;
    return {quotient, remainder, offset,
            offset + 0x424f7e98u, offset + 0x424f7ea9u};
}

ShotResultPrefixStatus store_shot_result_index_prefix(
    ShotResultFrameView frame,
    std::uint32_t index_word,
    std::uint32_t slot_word_observation,
    std::uint8_t slot_byte_11_observation) noexcept
{
    if (!valid(frame))
        return ShotResultPrefixStatus::invalid_view;

    const ShotResultIndex index = shot_result_index(index_word);
    frame.data[0xd0] = static_cast<std::uint8_t>(slot_word_observation == index_word);
    write_word(frame.data + 0x154, index.byte_offset);
    frame.data[0xef] = static_cast<std::uint8_t>(slot_byte_11_observation != 0);
    return ShotResultPrefixStatus::stored;
}

ShotResultConsumerExit consume_shot_result_postcall(
    ShotResultFrameView postcall_frame) noexcept
{
    if (!valid(postcall_frame))
        return ShotResultConsumerExit::invalid_view;

    auto* const data = postcall_frame.data;
    data[0xd0] = static_cast<std::uint8_t>(data[0xd0] & data[0xef]);
    const std::uint32_t address = read_word(data + 0x154) + 0x424f7eacu;
    write_word(data + 0x78, address);
    write_word(data + 0xf0, 0x7f7fffffu);
    write_word(data + 0x120, 0);
    return data[0xdc] == 0 ? ShotResultConsumerExit::branch_414c2233
                           : ShotResultConsumerExit::next_414c2003;
}

} // namespace rage::offline

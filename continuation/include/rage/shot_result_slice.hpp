#ifndef RAGE_SHOT_RESULT_SLICE_HPP
#define RAGE_SHOT_RESULT_SLICE_HPP

#include <cstddef>
#include <cstdint>

namespace rage::offline {

inline constexpr std::size_t shot_result_frame_bytes = 0x158;

struct ShotResultFrameView {
    std::uint8_t* data;
    std::size_t size;
};

struct ShotResultIndex {
    std::int32_t quotient;
    std::int32_t remainder;
    std::uint32_t byte_offset;
    std::uint32_t word_address;
    std::uint32_t byte_11_address;
};

// Signed truncation toward zero, not a nonnegative ring index. Addresses are raw32.
ShotResultIndex shot_result_index(std::uint32_t index_word) noexcept;

enum class ShotResultPrefixStatus {
    stored,
    invalid_view
};

// 0x414C197D..0x414C19C7. Inputs are observations at the resolved addresses,
// not an asserted command/object identity. Short/null rejection is adapter-only.
ShotResultPrefixStatus store_shot_result_index_prefix(
    ShotResultFrameView frame,
    std::uint32_t index_word,
    std::uint32_t slot_word_observation,
    std::uint8_t slot_byte_11_observation) noexcept;

enum class ShotResultConsumerExit {
    next_414c2003,
    branch_414c2233,
    invalid_view
};

// 0x414C1FC6..0x414C2003. Supply a fresh post-0x414BD940 frame observation;
// no values or identity are carried from store_shot_result_index_prefix.
ShotResultConsumerExit consume_shot_result_postcall(
    ShotResultFrameView postcall_frame) noexcept;

} // namespace rage::offline
#endif

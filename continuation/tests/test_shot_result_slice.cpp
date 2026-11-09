#include "rage/shot_result_slice.hpp"

#include <array>
#include <cstdint>
#include <iostream>

namespace {

unsigned assertions = 0;

#define CHECK(condition) do { \
    ++assertions; \
    if (!(condition)) { \
        std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; \
        return false; \
    } \
} while (0)

using Frame = std::array<std::uint8_t, rage::offline::shot_result_frame_bytes + 8>;

std::uint32_t read_word(const Frame& frame, std::size_t offset)
{
    return std::uint32_t{frame[offset]} | (std::uint32_t{frame[offset + 1]} << 8) |
           (std::uint32_t{frame[offset + 2]} << 16) |
           (std::uint32_t{frame[offset + 3]} << 24);
}

void put_word(Frame& frame, std::size_t offset, std::uint32_t word)
{
    for (unsigned i = 0; i < 4; ++i)
        frame[offset + i] = static_cast<std::uint8_t>(word >> (8 * i));
}

bool signed_index_boundaries()
{
    struct Case {
        std::uint32_t word;
        std::int32_t quotient;
        std::int32_t remainder;
    };
    constexpr std::array<Case, 17> cases{{
        {0, 0, 0}, {1, 0, 1}, {149, 0, 149}, {150, 1, 0}, {151, 1, 1},
        {299, 1, 149}, {300, 2, 0},
        {0xffffffffu, 0, -1}, {0xffffff6bu, 0, -149},
        {0xffffff6au, -1, 0}, {0xffffff69u, -1, -1},
        {0xfffffed5u, -1, -149}, {0xfffffed4u, -2, 0},
        {0x7fffffffu, 14316557, 97}, {0x7ffffffeu, 14316557, 96},
        {0x80000000u, -14316557, -98}, {0x80000001u, -14316557, -97}
    }};
    for (const auto& c : cases) {
        const auto result = rage::offline::shot_result_index(c.word);
        CHECK(result.quotient == c.quotient);
        CHECK(result.remainder == c.remainder);
        const auto offset = static_cast<std::uint32_t>(c.remainder) * 152u;
        CHECK(result.byte_offset == offset);
        CHECK(result.word_address == 0x424f7e98u + offset);
        CHECK(result.byte_11_address == result.word_address + 0x11u);
    }
    CHECK(rage::offline::shot_result_index(0xffffffffu).byte_offset == 0xffffff68u);
    CHECK(rage::offline::shot_result_index(0xffffffffu).word_address == 0x424f7e00u);
    return true;
}

bool prefix_stores_only_proved_bytes()
{
    for (const auto index : {0u, 149u, 150u, 0xffffffffu, 0x80000000u}) {
        for (const bool equal : {false, true}) {
            for (const std::uint8_t byte : {0u, 1u, 0x80u, 0xffu}) {
                Frame frame;
                frame.fill(0xa5);
                auto expected = frame;
                expected[0xd0] = static_cast<std::uint8_t>(equal);
                put_word(expected, 0x154, rage::offline::shot_result_index(index).byte_offset);
                expected[0xef] = static_cast<std::uint8_t>(byte != 0);
                const auto result = rage::offline::store_shot_result_index_prefix(
                    {frame.data(), rage::offline::shot_result_frame_bytes}, index,
                    equal ? index : index ^ 0x80000000u, byte);
                CHECK(result == rage::offline::ShotResultPrefixStatus::stored);
                CHECK(frame == expected);
                CHECK(read_word(frame, 0xd0) == (0xa5a5a500u | expected[0xd0]));
            }
        }
    }
    return true;
}

bool consumer_uses_distinct_postcall_raw_observations()
{
    Frame producer;
    producer.fill(0x5a);
    CHECK(rage::offline::store_shot_result_index_prefix(
        {producer.data(), producer.size()}, 151, 151, 1) ==
        rage::offline::ShotResultPrefixStatus::stored);
    const auto producer_before = producer;

    struct Case {
        std::uint8_t d0;
        std::uint8_t ef;
        std::uint8_t dc;
        std::uint32_t offset;
        std::uint32_t address;
    };
    constexpr std::array<Case, 5> cases{{
        {1, 1, 0, 0x98, 0x424f7f44},
        {1, 0, 0x80, 0, 0x424f7eac},
        {0, 1, 1, 0xffffff68u, 0x424f7e14},
        {0x80, 0x7f, 0, 0xffffffffu, 0x424f7eab},
        {0xaa, 0xf0, 0xff, 0xc0000000u, 0x024f7eac}
    }};
    for (const auto& c : cases) {
        Frame postcall;
        postcall.fill(0xa7);
        postcall[0xd0] = c.d0;
        postcall[0xef] = c.ef;
        postcall[0xdc] = c.dc;
        put_word(postcall, 0x154, c.offset);
        auto expected = postcall;
        expected[0xd0] = static_cast<std::uint8_t>(c.d0 & c.ef);
        put_word(expected, 0x78, c.address);
        put_word(expected, 0xf0, 0x7f7fffff);
        put_word(expected, 0x120, 0);
        const auto exit = rage::offline::consume_shot_result_postcall(
            {postcall.data(), rage::offline::shot_result_frame_bytes});
        CHECK(exit == (c.dc == 0 ? rage::offline::ShotResultConsumerExit::branch_414c2233
                                : rage::offline::ShotResultConsumerExit::next_414c2003));
        CHECK(postcall == expected);
        CHECK(read_word(postcall, 0x78) == c.address);
        CHECK(read_word(postcall, 0xf0) == 0x7f7fffff);
        CHECK(read_word(postcall, 0x120) == 0);
        CHECK(producer == producer_before);
    }
    return true;
}

bool invalid_views_are_adapter_only()
{
    Frame frame;
    frame.fill(0xdd);
    const auto before = frame;
    for (const rage::offline::ShotResultFrameView view : {
             rage::offline::ShotResultFrameView{nullptr, frame.size()},
             rage::offline::ShotResultFrameView{frame.data(), 0},
             rage::offline::ShotResultFrameView{
                 frame.data(), rage::offline::shot_result_frame_bytes - 1}}) {
        CHECK(rage::offline::store_shot_result_index_prefix(view, 1, 1, 1) ==
              rage::offline::ShotResultPrefixStatus::invalid_view);
        CHECK(frame == before);
        CHECK(rage::offline::consume_shot_result_postcall(view) ==
              rage::offline::ShotResultConsumerExit::invalid_view);
        CHECK(frame == before);
    }
    return true;
}

} // namespace

int main()
{
    if (!signed_index_boundaries() || !prefix_stores_only_proved_bytes() ||
        !consumer_uses_distinct_postcall_raw_observations() ||
        !invalid_views_are_adapter_only())
        return 1;
    std::cout << "PASS shot_result_slice: " << assertions << " assertions\n";
}

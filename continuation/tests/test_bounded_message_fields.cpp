#include "rage/bounded_message_fields.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

using namespace rage::offline;

namespace {
int assertions = 0;
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; \
    std::exit(1); } } while (false)

std::uint32_t bits(float value)
{
    std::uint32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

struct BitFixture {
    std::vector<std::uint8_t> bytes;
    std::int32_t length{};

    void put(std::uint32_t value, int count)
    {
        for (int i = 0; i < count; ++i) {
            if (length % 8 == 0) {
                bytes.push_back(0);
            }
            bytes[static_cast<std::size_t>(length / 8)] |=
                static_cast<std::uint8_t>(((value >> i) & 1u) << (length % 8));
            ++length;
        }
    }

    void coordinate(bool integer, bool fraction, bool negative,
                    std::uint32_t integer_bits, std::uint32_t fraction_bits)
    {
        put(integer, 1);
        put(fraction, 1);
        if (integer || fraction) {
            put(negative, 1);
            if (integer) put(integer_bits, 14);
            if (fraction) put(fraction_bits, 5);
        }
    }

    MessageBitCursor reader(std::int32_t start = 0) const
    {
        return {bytes.data(), bytes.size(), length, start, false};
    }
};

constexpr std::uint32_t word = UINT32_C(0x87654321);

void header(BitFixture& fixture, std::uint32_t tag = 100, std::uint32_t index = 63)
{
    fixture.put(word, 32);
    fixture.put(index, 7);
    fixture.put(tag, 32);
}

void zero_body(BitFixture& fixture, std::uint32_t extra = 0)
{
    for (int i = 0; i < 3; ++i) fixture.coordinate(false, false, false, 0, 0);
    fixture.put(extra, 7);
}

void fields_order_and_cursor()
{
    // Includes both DWORD-aligned and crossing reads, without padded storage.
    for (const auto start : {0, 1, 7, 31, 32}) {
        BitFixture fixture;
        fixture.put(0, start);
        header(fixture, 100, 127);
        fixture.coordinate(true, true, true, 16383, 31); // z
        fixture.coordinate(false, true, true, 0, 0); // y = -0
        fixture.coordinate(true, false, false, 0, 0); // x = 1
        fixture.put(127, 7);
        const auto end = fixture.length;
        fixture.put(0x5a, 8);
        auto reader = fixture.reader(start);
        const auto result = decode_bounded_message_fields(reader, {word, 100, 0});
        CHECK(result.effect == MessageFieldEffect::ready_for_consumer);
        CHECK(result.fields.first_word == word);
        CHECK(result.fields.zero_based_index + 1 == 128);
        CHECK(result.fields.numeric_tag == 100);
        CHECK(bits(result.fields.position.z) == bits(-16384.96875f));
        CHECK(bits(result.fields.position.y) == UINT32_C(0x80000000));
        CHECK(bits(result.fields.position.x) == bits(1.0f));
        CHECK(result.fields.extra_bits == 127);
        CHECK(reader.position == end);
        CHECK(!reader.overflow);
    }
}

void branch_order_and_signed_window()
{
    struct Case { std::uint32_t tag; std::uint32_t reference;
                  std::int32_t tolerance; bool accepted; };
    const std::array<Case, 13> cases{{
        {100, 100, 0, true}, {103, 100, 3, true}, {97, 100, 3, true},
        {104, 100, 3, false}, {96, 100, 3, false}, {0, 0, -1, false},
        {0, UINT32_C(0xffffffff), 1, true},
        {UINT32_C(0xffffffff), 0, 1, true},
        {UINT32_C(0x80000000), 0, 0, true},
        {0, UINT32_C(0x80000000), std::numeric_limits<std::int32_t>::min(), true},
        {UINT32_C(0x7fffffff), 0, std::numeric_limits<std::int32_t>::max(), true},
        {UINT32_C(0x80000001), 0, std::numeric_limits<std::int32_t>::max(), true},
        {1, 0, -1, false}
    }};
    for (const auto& test : cases) {
        BitFixture fixture;
        header(fixture, test.tag, 0);
        zero_body(fixture);
        auto reader = fixture.reader();
        const auto result = decode_bounded_message_fields(
            reader, {word, test.reference, test.tolerance});
        CHECK(result.effect == (test.accepted ? MessageFieldEffect::ready_for_consumer :
                                                MessageFieldEffect::tag_outside_window));
        CHECK(reader.position == (test.accepted ? fixture.length : 71));
        CHECK(result.fields.numeric_tag == test.tag);
        CHECK(result.fields.zero_based_index == 0);
        CHECK(!reader.overflow);
    }

    BitFixture fixture;
    header(fixture);
    zero_body(fixture);
    auto reader = fixture.reader();
    auto result = decode_bounded_message_fields(reader, {word ^ 1u, 100, 0});
    CHECK(result.effect == MessageFieldEffect::first_word_mismatch);
    CHECK(reader.position == 32);
    CHECK(result.fields.numeric_tag == 0);

    reader = fixture.reader();
    reader.bit_limit = 0;
    result = decode_bounded_message_fields(reader, {1, 100, 0});
    CHECK(result.effect == MessageFieldEffect::first_word_mismatch);
    CHECK(reader.overflow);
    CHECK(reader.position == 0);
    reader = fixture.reader();
    reader.bit_limit = 0;
    result = decode_bounded_message_fields(reader, {0, 100, 0});
    CHECK(result.effect == MessageFieldEffect::tag_outside_window);
    CHECK(reader.overflow);
}

void coordinate_branches_and_truncation()
{
    for (const bool integer : {false, true}) {
        for (const bool fraction : {false, true}) {
            for (const bool negative : {false, true}) {
                BitFixture fixture;
                header(fixture);
                fixture.coordinate(integer, fraction, negative, 16383, 31);
                fixture.coordinate(false, false, false, 0, 0);
                fixture.coordinate(false, false, false, 0, 0);
                fixture.put(1, 7);
                auto reader = fixture.reader();
                auto result = decode_bounded_message_fields(reader, {word, 100, 0});
                float expected = (integer ? 16384.0f : 0.0f) +
                                 (fraction ? 0.96875f : 0.0f);
                if (negative && (integer || fraction)) expected = -expected;
                CHECK(result.effect == MessageFieldEffect::ready_for_consumer);
                CHECK(bits(result.fields.position.z) == bits(expected));
                CHECK(result.fields.position.x == 0);
                CHECK(result.fields.position.y == 0);
                CHECK(reader.position == fixture.length);
            }
        }
    }

    BitFixture fixture;
    header(fixture);
    fixture.coordinate(true, true, false, 20, 17);
    fixture.coordinate(true, false, true, 0, 0);
    fixture.coordinate(false, true, false, 0, 8);
    fixture.put(42, 7);
    for (std::int32_t limit = 71; limit < fixture.length; ++limit) {
        auto reader = fixture.reader();
        reader.bit_limit = limit;
        const auto result = decode_bounded_message_fields(reader, {word, 100, 0});
        CHECK(result.effect == MessageFieldEffect::read_overflow);
        CHECK(reader.overflow);
        CHECK(reader.position == limit);
        if (limit == 74) {
            CHECK(result.fields.position.z == 1.0f);
        }
    }
    auto reader = fixture.reader();
    reader.overflow = true;
    const auto result = decode_bounded_message_fields(reader, {word, 100, 0});
    CHECK(result.effect == MessageFieldEffect::read_overflow);
    CHECK(result.fields.extra_bits == 42);
    CHECK(reader.position == fixture.length);

    reader = {nullptr, 0, 0, 0, false};
    CHECK(decode_bounded_message_fields(reader, {0, 0, 0}).effect ==
          MessageFieldEffect::read_overflow);
    CHECK(reader.overflow);
}

void invalid_views_are_offline_guards()
{
    const std::uint8_t byte = 0;
    for (auto reader : std::array<MessageBitCursor, 7>{{
             {nullptr, 1, 1, 0, false}, {&byte, 0, 1, 0, false},
             {&byte, 1, -1, 0, false}, {&byte, 1, 8, -1, false},
             {&byte, 1, 8, 9, false}, {&byte, 1, 9, 0, false},
             {&byte, 1, std::numeric_limits<std::int32_t>::max(), 0, false}
         }}) {
        const auto old = reader;
        CHECK(decode_bounded_message_fields(reader, {0, 0, 0}).effect ==
              MessageFieldEffect::invalid_bit_view);
        CHECK(reader.position == old.position);
        CHECK(reader.overflow == old.overflow);
    }
}
using PrefixEdge = MessageReceiverPrefixEdge;

MessageReceiverPrefixObservations prefix_observations()
{
    return {7u, 0u, std::nullopt, std::nullopt, 0x200u,
            MessageReceiverEpochObservation{5u, 5u}, 0x1000u, 1u};
}

void receiver_prefix_gates_and_writes()
{
    auto observation = prefix_observations();
    MessageReceiverPrefixState state{0xaabbccddu, 42u};
    const auto run = [&](std::uint32_t index = 64) {
        return evaluate_message_receiver_prefix(index, observation, state);
    };
    observation = {};
    CHECK(run(65).edge == PrefixEdge::index_above_64);
    CHECK(run(0x7fffffffu).edge == PrefixEdge::index_above_64);
    CHECK(state.scratch_1c == 0xaabbccddu);
    CHECK(state.exception_state_128 == 42u);
    for (const auto index : {0u, 64u, 0x80000000u, 0xffffffffu}) {
        CHECK(run(index).edge == PrefixEdge::dispatch_return_unobserved);
    }
    observation.dispatch_return_eax = 64;
    CHECK(run().edge == PrefixEdge::index_matches_dispatch);
    observation.dispatch_return_eax = 7;
    CHECK(run().edge == PrefixEdge::guard_offset_unobserved);
    CHECK(state.scratch_1c == 0xaabbccddu);
    observation.guard_offset_word = 0x80;
    CHECK(run().edge == PrefixEdge::guard_epoch_unobserved);
    CHECK(state.scratch_1c == 0x80);
    CHECK(state.exception_state_128 == 42u);

    observation.guard_epoch = MessageReceiverEpochObservation{1, 0};
    CHECK(run().edge == PrefixEdge::guard_epoch_side_exit);
    observation.guard_epoch = MessageReceiverEpochObservation{0x7fffffffu, 0x80000000u};
    CHECK(run().edge == PrefixEdge::guard_epoch_side_exit);
    observation.guard_epoch = MessageReceiverEpochObservation{0x80000000u, 0x7fffffffu};
    CHECK(run().edge == PrefixEdge::guard_word_unobserved);
    observation.guard_epoch = MessageReceiverEpochObservation{0xffffffffu, 0xffffffffu};
    CHECK(run().edge == PrefixEdge::guard_word_unobserved);
    observation.guard_word = 0x80000000u;
    CHECK(run().edge == PrefixEdge::guard_word_nonzero);
    CHECK(state.exception_state_128 == 42u);
    observation.guard_word = 0;
    CHECK(run().edge == PrefixEdge::source_return_unobserved);
    CHECK(state.exception_state_128 == 0xffffffffu);
    CHECK(state.scratch_1c == 0x80);
    observation.source_return_eax = 0;
    CHECK(run().edge == PrefixEdge::source_zero);
    CHECK(state.scratch_1c == 0);
    observation.source_return_eax = 0x200;
    CHECK(run().edge == PrefixEdge::slot_epoch_unobserved);
    CHECK(state.scratch_1c == 0x200);

    observation.slot_epoch = MessageReceiverEpochObservation{0, 0xffffffffu};
    CHECK(run().edge == PrefixEdge::slot_epoch_side_exit);
    observation.slot_epoch = MessageReceiverEpochObservation{0xffffffffu, 0};
    CHECK(run().edge == PrefixEdge::slot_base_unobserved);
    observation.slot_base_word = 0;
    CHECK(run().edge == PrefixEdge::slot_base_zero_path);
    CHECK(!run().slot_address);
    observation.slot_base_word = 0x1000;
    CHECK(run().edge == PrefixEdge::slot_byte_unobserved);
    CHECK(run().slot_address == 0x1240u);
    observation.slot_byte = 0;
    CHECK(run().edge == PrefixEdge::slot_byte_zero);
    CHECK(run().slot_address == 0x1240u);
    for (const auto byte : {1u, 128u, 255u}) {
        observation.slot_byte = static_cast<std::uint8_t>(byte);
        CHECK(run().edge == PrefixEdge::next_epoch_gate);
    }

    observation = prefix_observations();
    observation.guard_epoch = MessageReceiverEpochObservation{1, 0};
    observation.guard_word = 123;
    CHECK(run().edge == PrefixEdge::next_epoch_gate); // zero offset skips guard
    observation.slot_base_word = 0xfffffff0u;
    observation.source_return_eax = 0x20;
    CHECK(run().slot_address == 0x50u);
    CHECK(run(0xffffffffu).slot_address == 0x0fu);
    observation.dispatch_return_eax = 0xffffffffu;
    CHECK(run(0xffffffffu).edge == PrefixEdge::index_matches_dispatch);
}

void parsed_index_reaches_conditional_receiver_prefix()
{
    const auto observation = prefix_observations();
    for (std::uint32_t index = 0; index < 128; ++index) {
        BitFixture fixture;
        header(fixture, 100, index);
        zero_body(fixture);
        auto reader = fixture.reader();
        const auto decoded = decode_bounded_message_fields(reader, {word, 100, 0});
        CHECK(decoded.effect == MessageFieldEffect::ready_for_consumer);
        MessageReceiverPrefixState state{9u, 42u};
        const auto result = evaluate_message_receiver_prefix(
            decoded.fields.zero_based_index + 1u, observation, state);
        const auto expected = index >= 64 ? PrefixEdge::index_above_64 :
            index == 6 ? PrefixEdge::index_matches_dispatch : PrefixEdge::next_epoch_gate;
        CHECK(result.edge == expected);
        if (expected == PrefixEdge::next_epoch_gate) {
            CHECK(result.slot_address == 0x1201u + index);
            CHECK(state.scratch_1c == 0x200);
            CHECK(state.exception_state_128 == 0xffffffffu);
        } else {
            CHECK(!result.slot_address);
            CHECK(state.scratch_1c == 9u);
            CHECK(state.exception_state_128 == 42u);
        }
    }
}
} // namespace

int main()
{
    fields_order_and_cursor();
    branch_order_and_signed_window();
    coordinate_branches_and_truncation();
    invalid_views_are_offline_guards();
    receiver_prefix_gates_and_writes();
    parsed_index_reaches_conditional_receiver_prefix();
    std::cout << "bounded_message_fields: " << assertions << " assertions passed\n";
}

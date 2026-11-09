#include "rage/history_slice.hpp"

#include <array>
#include <cstdint>
#include <cstring>
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

enum class ExpectedSource {
    record,
    local_word,
    zero
};

struct ExpectedStore {
    ExpectedSource source;
    std::size_t source_offset;
    std::size_t destination_offset;
};

constexpr std::array<ExpectedStore, 23> expected_stores{{
    {ExpectedSource::record, 0x10, 0x00},
    {ExpectedSource::record, 0x14, 0x04},
    {ExpectedSource::record, 0x18, 0x08},
    {ExpectedSource::record, 0x7c, 0x24},
    {ExpectedSource::record, 0x80, 0x28},
    {ExpectedSource::record, 0x84, 0x2c},
    {ExpectedSource::record, 0x88, 0x30},
    {ExpectedSource::record, 0x8c, 0x34},
    {ExpectedSource::record, 0x90, 0x38},
    {ExpectedSource::record, 0x38, 0x80},
    {ExpectedSource::record, 0x4c, 0x94},
    {ExpectedSource::record, 0x1c, 0x18},
    {ExpectedSource::record, 0x20, 0x1c},
    {ExpectedSource::record, 0x24, 0x20},
    {ExpectedSource::local_word, 0, 0x3c},
    {ExpectedSource::record, 0x94, 0xa0},
    {ExpectedSource::record, 0x98, 0x70},
    {ExpectedSource::record, 0x28, 0x64},
    {ExpectedSource::record, 0x2c, 0x68},
    {ExpectedSource::record, 0x30, 0x6c},
    {ExpectedSource::record, 0x5c, 0x98},
    {ExpectedSource::zero, 0, 0xb0},
    {ExpectedSource::record, 0x40, 0x40}
}};

void put_word(std::uint8_t* bytes, std::size_t offset, std::uint32_t word)
{
    std::memcpy(bytes + offset, &word, sizeof(word));
}

std::uint32_t get_word(const std::uint8_t* bytes, std::size_t offset)
{
    std::uint32_t word;
    std::memcpy(&word, bytes + offset, sizeof(word));
    return word;
}

void apply_expected(const std::uint8_t* source, std::uint8_t* destination,
                    std::uint32_t local_word)
{
    for (const ExpectedStore& store : expected_stores) {
        std::uint32_t word = 0;
        if (store.source == ExpectedSource::record)
            std::memcpy(&word, source + store.source_offset, sizeof(word));
        else if (store.source == ExpectedSource::local_word)
            word = local_word;
        std::memcpy(destination + store.destination_offset, &word, sizeof(word));
    }
}

bool projected_fields_preserve_bits_and_order()
{
    std::array<std::uint8_t, rage::offline::history_slice_source_bytes> source{};
    std::array<std::uint8_t, rage::offline::history_slice_destination_bytes> destination{};
    source.fill(0x37);
    destination.fill(0xa5);
    put_word(source.data(), 0x10, 0x7fc12345u);
    put_word(source.data(), 0x14, 0x80000000u);
    put_word(source.data(), 0x18, 0x00000000u);
    put_word(source.data(), 0x38, 0xdeadbeefu);
    put_word(source.data(), 0x40, 0x12345678u);
    const auto source_before = source;
    auto expected = destination;
    constexpr std::uint32_t local_word = 0xcafebabeu;
    apply_expected(source.data(), expected.data(), local_word);

    const auto result = rage::offline::copy_history_store_prefix(
        {source.data(), source.size()}, {destination.data(), destination.size()},
        local_word);
    CHECK(result == rage::offline::HistorySliceStatus::copied);
    CHECK(destination == expected);
    CHECK(source == source_before);
    CHECK(get_word(destination.data(), 0x00) == 0x7fc12345u);
    CHECK(get_word(destination.data(), 0x04) == 0x80000000u);
    CHECK(get_word(destination.data(), 0x08) == 0x00000000u);
    CHECK(get_word(destination.data(), 0x3c) == local_word);
    CHECK(get_word(destination.data(), 0x80) == 0xdeadbeefu);
    CHECK(get_word(destination.data(), 0xb0) == 0);
    return true;
}

bool invalid_views_do_not_write()
{
    std::array<std::uint8_t, rage::offline::history_slice_source_bytes> source{};
    std::array<std::uint8_t, rage::offline::history_slice_destination_bytes> destination{};
    destination.fill(0x6d);
    const auto before = destination;

    CHECK(rage::offline::copy_history_store_prefix(
              {source.data(), source.size() - 1},
              {destination.data(), destination.size()}, 0) ==
          rage::offline::HistorySliceStatus::invalid_view);
    CHECK(destination == before);
    CHECK(rage::offline::copy_history_store_prefix(
              {nullptr, source.size()}, {destination.data(), destination.size()}, 0) ==
          rage::offline::HistorySliceStatus::invalid_view);
    CHECK(destination == before);
    CHECK(rage::offline::copy_history_store_prefix(
              {source.data(), source.size()},
              {destination.data(), destination.size() - 1}, 0) ==
          rage::offline::HistorySliceStatus::invalid_view);
    CHECK(destination == before);
    CHECK(rage::offline::copy_history_store_prefix(
              {source.data(), source.size()}, {nullptr, destination.size()}, 0) ==
          rage::offline::HistorySliceStatus::invalid_view);
    CHECK(destination == before);
    return true;
}

bool overlapping_views_follow_store_order()
{
    std::array<std::uint8_t, rage::offline::history_slice_destination_bytes> actual{};
    for (std::size_t i = 0; i < actual.size(); ++i)
        actual[i] = static_cast<std::uint8_t>(i * 13u + 5u);
    auto expected = actual;
    constexpr std::uint32_t local_word = 0x87654321u;
    apply_expected(expected.data(), expected.data(), local_word);

    CHECK(rage::offline::copy_history_store_prefix(
              {actual.data(), rage::offline::history_slice_source_bytes},
              {actual.data(), actual.size()}, local_word) ==
          rage::offline::HistorySliceStatus::copied);
    CHECK(actual == expected);
    return true;
}

} // namespace

int main()
{
    if (!projected_fields_preserve_bits_and_order())
        return 1;
    if (!invalid_views_do_not_write())
        return 1;
    if (!overlapping_views_follow_store_order())
        return 1;
    std::cout << "PASS history slice: " << assertions << " assertions\n";
    return 0;
}

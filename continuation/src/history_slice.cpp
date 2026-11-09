#include "rage/history_slice.hpp"

#include <array>
#include <cstring>

namespace rage::offline {
namespace {

enum class WordSource {
    record,
    local_word,
    zero
};

enum class WordKind {
    float32_bits,
    opaque32,
    literal32
};

struct Store {
    WordSource source;
    std::size_t source_offset;
    std::size_t destination_offset;
    WordKind kind;
};

constexpr std::array<Store, 23> stores{{
    {WordSource::record, 0x10, 0x00, WordKind::float32_bits},
    {WordSource::record, 0x14, 0x04, WordKind::float32_bits},
    {WordSource::record, 0x18, 0x08, WordKind::float32_bits},
    {WordSource::record, 0x7c, 0x24, WordKind::float32_bits},
    {WordSource::record, 0x80, 0x28, WordKind::float32_bits},
    {WordSource::record, 0x84, 0x2c, WordKind::float32_bits},
    {WordSource::record, 0x88, 0x30, WordKind::float32_bits},
    {WordSource::record, 0x8c, 0x34, WordKind::float32_bits},
    {WordSource::record, 0x90, 0x38, WordKind::float32_bits},
    {WordSource::record, 0x38, 0x80, WordKind::opaque32},
    {WordSource::record, 0x4c, 0x94, WordKind::float32_bits},
    {WordSource::record, 0x1c, 0x18, WordKind::float32_bits},
    {WordSource::record, 0x20, 0x1c, WordKind::float32_bits},
    {WordSource::record, 0x24, 0x20, WordKind::float32_bits},
    {WordSource::local_word, 0, 0x3c, WordKind::opaque32},
    {WordSource::record, 0x94, 0xa0, WordKind::float32_bits},
    {WordSource::record, 0x98, 0x70, WordKind::float32_bits},
    {WordSource::record, 0x28, 0x64, WordKind::float32_bits},
    {WordSource::record, 0x2c, 0x68, WordKind::float32_bits},
    {WordSource::record, 0x30, 0x6c, WordKind::float32_bits},
    {WordSource::record, 0x5c, 0x98, WordKind::float32_bits},
    {WordSource::zero, 0, 0xb0, WordKind::literal32},
    {WordSource::record, 0x40, 0x40, WordKind::opaque32}
}};

constexpr std::size_t count_kind(WordKind kind)
{
    std::size_t count = 0;
    for (const Store& store : stores)
        count += store.kind == kind ? 1 : 0;
    return count;
}

static_assert(sizeof(std::uint32_t) == 4);
static_assert(count_kind(WordKind::float32_bits) == 19);
static_assert(count_kind(WordKind::opaque32) == 3);
static_assert(count_kind(WordKind::literal32) == 1);

} // namespace

HistorySliceStatus copy_history_store_prefix(
    HistorySourceBytes source,
    HistoryDestinationBytes destination,
    std::uint32_t local_word_at_ebp_minus_38) noexcept
{
    if (source.data == nullptr || source.size < history_slice_source_bytes ||
        destination.data == nullptr ||
        destination.size < history_slice_destination_bytes)
        return HistorySliceStatus::invalid_view;

    for (const Store& store : stores) {
        std::uint32_t word = 0;
        if (store.source == WordSource::record) {
            std::memcpy(&word, source.data + store.source_offset, sizeof(word));
        } else if (store.source == WordSource::local_word) {
            word = local_word_at_ebp_minus_38;
        }
        std::memcpy(destination.data + store.destination_offset, &word, sizeof(word));
    }
    return HistorySliceStatus::copied;
}

} // namespace rage::offline

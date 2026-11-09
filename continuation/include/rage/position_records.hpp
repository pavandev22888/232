#ifndef RAGE_POSITION_RECORDS_HPP
#define RAGE_POSITION_RECORDS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace rage::offline {

struct Vec3 {
    float x;
    float y;
    float z;
};

// N, not the target T or a pose-history record. No host pointers in this layout.
struct PositionRecord {
    std::uint8_t active;
    std::array<std::uint8_t, 3> reserved_01;
    std::uint32_t flags;
    std::int32_t tag;
    float updated_at;
    std::array<std::uint8_t, 8> opaque_10;
    float update_gap;
    std::int32_t item_definition;
    std::int32_t update_type;
    Vec3 position;
    Vec3 actor_position;
    Vec3 snapshot_position;
    float validity_marker;
    std::array<std::uint8_t, 12> opaque_4c;
    std::int32_t sequence;
    std::int32_t previous_sequence;
    std::int32_t snapshot_sequence;
    std::int32_t previous_snapshot_sequence;
    std::int32_t snapshot_type;
    std::int32_t snapshot_tick;
    std::int32_t updated_tick;
};

static_assert(std::is_standard_layout_v<PositionRecord>);
static_assert(std::is_trivially_copyable_v<PositionRecord>);
static_assert(sizeof(Vec3) == 12);
static_assert(sizeof(PositionRecord) == 0x74);
static_assert(offsetof(PositionRecord, flags) == 0x04);
static_assert(offsetof(PositionRecord, tag) == 0x08);
static_assert(offsetof(PositionRecord, updated_at) == 0x0c);
static_assert(offsetof(PositionRecord, update_gap) == 0x18);
static_assert(offsetof(PositionRecord, item_definition) == 0x1c);
static_assert(offsetof(PositionRecord, update_type) == 0x20);
static_assert(offsetof(PositionRecord, position) == 0x24);
static_assert(offsetof(PositionRecord, actor_position) == 0x30);
static_assert(offsetof(PositionRecord, snapshot_position) == 0x3c);
static_assert(offsetof(PositionRecord, validity_marker) == 0x48);
static_assert(offsetof(PositionRecord, sequence) == 0x58);
static_assert(offsetof(PositionRecord, previous_sequence) == 0x5c);
static_assert(offsetof(PositionRecord, snapshot_sequence) == 0x60);
static_assert(offsetof(PositionRecord, previous_snapshot_sequence) == 0x64);
static_assert(offsetof(PositionRecord, snapshot_type) == 0x68);
static_assert(offsetof(PositionRecord, snapshot_tick) == 0x6c);
static_assert(offsetof(PositionRecord, updated_tick) == 0x70);

// Owns the offline records. This is not the original allocator or manager ABI.
class PositionStore {
public:
    static constexpr std::size_t capacity = 2048;
    PositionStore();
    PositionRecord& at(std::size_t index) { return records_.at(index); }
    const PositionRecord& at(std::size_t index) const { return records_.at(index); }

private:
    std::array<PositionRecord, capacity> records_{};
};

struct ResolvedUpdate {
    std::int32_t sequence;
    std::int32_t type;
    std::int32_t tag;
    std::int32_t tick;
    float now;
    float interval;
    Vec3 position;
};

// Supplied observations, not a replacement for the unresolved trace/provider.
struct TraceObservation {
    float fraction;
    bool all_solid;
    bool start_solid;
    Vec3 end_position;
    std::uint32_t provided_flags;
};

enum class UpdateEffect {
    applied,
    higher_type_at_same_time,
    older_type_two_sequence,
    all_solid,
    start_solid,
    full_or_unordered_trace
};

// Starts at 4150B3AC after external gates. Diagnostic result is not original AL.
UpdateEffect update_resolved_record(PositionRecord& record,
                                    const ResolvedUpdate& update,
                                    const TraceObservation& trace);

// Only the resolved-word store at 414F85A0; absent handle preserves the old field.
void store_resolved_item_definition(PositionRecord& record,
                                    std::optional<std::uint16_t> item_word);

struct ReaderOptions {
    bool primary_enabled;       // cfg +364
    bool alternate_enabled;     // cfg +365
    std::uint8_t select_alternate; // cfg +366, any nonzero byte
};

struct ResolvedRead {
    bool source_available;      // 41B6C670 result != 0
    bool slot_flag;             // resolved byte table, index <= 64
    std::int32_t slot_value;     // resolved signed DWORD table
    ReaderOptions options;
    float now;
};

enum class ReadStatus {
    usable,
    source_unavailable,
    cleared_by_policy,
    inactive,
    expired,
    outside_supported_domain
};

struct PositionRead {
    // Borrowed until the store is moved or destroyed, even when status is failure.
    PositionRecord* record;
    ReadStatus status;
};

// Index >=2048 is a host contract failure; the original handler is unresolved.
PositionRead read_resolved_record(PositionStore& store, std::uint32_t index,
                                  const ResolvedRead& input);

// Only the two tests at 413650E5..413650F7, not the rest of that consumer.
bool passes_item_consumer_gate(const PositionRead& read);

} // namespace rage::offline
#endif

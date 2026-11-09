#include "rage/position_records.hpp"

#include <cfenv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

using namespace rage::offline;
namespace {
int assertions = 0;
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; \
    std::exit(1); } } while (false)

const float nan = std::numeric_limits<float>::quiet_NaN();
const float infinity = std::numeric_limits<float>::infinity();

bool same_bytes(const PositionRecord& left, const PositionRecord& right)
{
    return std::memcmp(&left, &right, sizeof(left)) == 0;
}

bool same_position(Vec3 left, Vec3 right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

PositionRecord seed()
{
    PositionRecord record{};
    record.reserved_01 = {0xab, 0xcd, 0xef};
    record.opaque_10.fill(0xa5);
    record.opaque_4c.fill(0x5a);
    record.flags = 0x12345678;
    record.tag = 100;
    record.updated_at = 9.0f;
    record.update_gap = 99.0f;
    record.item_definition = 7;
    record.update_type = 1;
    record.position = {10.0f, 20.0f, 30.0f};
    record.actor_position = {-10.0f, -20.0f, -30.0f};
    record.snapshot_position = {1.0f, 2.0f, 3.0f};
    record.validity_marker = 1.0f;
    record.sequence = 4;
    record.previous_sequence = 3;
    record.snapshot_sequence = 2;
    record.previous_snapshot_sequence = 1;
    record.snapshot_type = 1;
    record.snapshot_tick = 90;
    record.updated_tick = 95;
    return record;
}

ResolvedUpdate incoming()
{
    return {5, 2, 77, 100, 10.0f, 1.0f / 64.0f, {40.0f, 50.0f, 60.0f}};
}

TraceObservation trace()
{
    return {0.75f, false, false, {70.0f, 80.0f, 90.0f}, 0xf6u};
}

ResolvedRead reading()
{
    return {true, true, 1, {true, false, 0}, 10.0f};
}

void initialization()
{
    PositionStore store;
    PositionRecord expected{};
    expected.tag = 100;
    for (std::size_t index = 0; index < PositionStore::capacity; ++index)
        CHECK(same_bytes(store.at(index), expected));
}

void update_lifecycle()
{
    auto record = seed();
    const auto old = record;
    const auto update = incoming();
    const auto observed_trace = trace();
    CHECK(update_resolved_record(record, update, observed_trace) == UpdateEffect::applied);
    auto expected = old;
    expected.active = 1;
    expected.flags = 0xf5u;
    expected.tag = 77;
    expected.updated_at = 10.0f;
    expected.update_gap = 1.0f;
    expected.update_type = 2;
    expected.position = observed_trace.end_position;
    expected.snapshot_position = old.position;
    expected.sequence = 5;
    expected.previous_sequence = 4;
    expected.snapshot_sequence = 5;
    expected.previous_snapshot_sequence = 2;
    expected.snapshot_type = 2;
    expected.snapshot_tick = 100;
    expected.updated_tick = 100;
    CHECK(same_bytes(record, expected));

    CHECK(update_resolved_record(record, update, observed_trace) == UpdateEffect::applied);
    expected.update_gap = 0.0f;
    CHECK(same_bytes(record, expected));
    auto next = update;
    next.sequence = 6;
    next.tick = 101;
    next.now = 11.0f;
    next.tag = -1;
    CHECK(update_resolved_record(record, next, observed_trace) == UpdateEffect::applied);
    CHECK(same_position(record.snapshot_position, observed_trace.end_position));
    CHECK(record.previous_sequence == 5 && record.previous_snapshot_sequence == 5);
    CHECK(record.snapshot_tick == 101 && record.updated_tick == 101 && record.tag == 77);
}

void rejected_updates_do_not_write()
{
    auto update = incoming();
    auto observed_trace = trace();
    auto record = seed();
    auto before = record;
    auto reject = [&](UpdateEffect effect) {
        CHECK(update_resolved_record(record, update, observed_trace) == effect);
        CHECK(same_bytes(record, before));
    };
    record.updated_at = update.now;
    record.update_type = 3;
    before = record;
    reject(UpdateEffect::higher_type_at_same_time);
    record.updated_at = nan;
    before = record;
    reject(UpdateEffect::higher_type_at_same_time);
    record = seed();
    update.now = nan;
    record.update_type = 3;
    before = record;
    reject(UpdateEffect::higher_type_at_same_time);

    record = seed();
    update = incoming();
    record.sequence = 6;
    before = record;
    reject(UpdateEffect::older_type_two_sequence);
    record = seed();
    before = record;
    observed_trace.all_solid = true;
    observed_trace.start_solid = true;
    reject(UpdateEffect::all_solid);
    observed_trace.all_solid = false;
    reject(UpdateEffect::start_solid);
    observed_trace.start_solid = false;
    for (float fraction : {1.0f, nan}) {
        observed_trace.fraction = fraction;
        reject(UpdateEffect::full_or_unordered_trace);
    }
}

void snapshot_rules()
{
    for (int same_sequence : {0, 1}) {
        for (int higher_type : {0, 1}) {
            auto record = seed();
            auto update = incoming();
            record.snapshot_sequence = same_sequence ? update.sequence : 2;
            record.snapshot_type = higher_type ? update.type - 1 : update.type;
            const auto before = record;
            CHECK(update_resolved_record(record, update, trace()) == UpdateEffect::applied);
            const bool replace = !same_sequence || higher_type;
            CHECK(same_position(record.snapshot_position,
                                replace ? before.position : before.snapshot_position));
            CHECK(record.snapshot_tick == (replace ? update.tick : before.snapshot_tick));
            CHECK(record.previous_snapshot_sequence ==
                  (same_sequence ? before.previous_snapshot_sequence : before.snapshot_sequence));
            CHECK(record.snapshot_sequence == update.sequence);
        }
    }
    auto record = seed();
    record.snapshot_position = record.position;
    const auto before = record;
    CHECK(update_resolved_record(record, incoming(), trace()) == UpdateEffect::applied);
    CHECK(record.snapshot_sequence == before.snapshot_sequence);
    CHECK(record.previous_snapshot_sequence == before.previous_snapshot_sequence);
    CHECK(record.snapshot_tick == before.snapshot_tick);

    record = seed();
    record.position = {nan, 0.0f, 0.0f};
    record.snapshot_position = {123.0f, -0.0f, 0.0f};
    CHECK(update_resolved_record(record, incoming(), trace()) == UpdateEffect::applied);
    CHECK(record.snapshot_sequence == 2 && record.snapshot_position.x == 123.0f);
    record = seed();
    record.position = {nan, 9.0f, 0.0f};
    record.snapshot_position = {123.0f, 0.0f, 0.0f};
    CHECK(update_resolved_record(record, incoming(), trace()) == UpdateEffect::applied);
    CHECK(record.snapshot_sequence == 5 && std::isnan(record.snapshot_position.x));
}

void numerical_boundaries()
{
    const auto update = incoming();
    for (float stamp : {5.0f, 6.0f, std::nextafter(6.0f, infinity), 9.0f, 11.0f, nan}) {
        auto record = seed();
        record.updated_at = stamp;
        CHECK(update_resolved_record(record, update, trace()) == UpdateEffect::applied);
        if (stamp <= 6.0f)
            CHECK(record.update_gap == 99.0f);
        else if (std::isnan(stamp))
            CHECK(std::isnan(record.update_gap));
        else
            CHECK(record.update_gap == 10.0f - stamp);
    }
    for (std::int32_t type : {0, 1, 2, 5, -1}) {
        for (float fraction : {0.25f, 0.5f, std::nextafter(0.5f, infinity),
                               std::nextafter(0.97f, 0.0f), 0.97f, 1.5f, -1.0f}) {
            auto record = seed();
            auto input = incoming();
            input.type = type;
            auto observed_trace = trace();
            observed_trace.fraction = fraction;
            CHECK(update_resolved_record(record, input, observed_trace) == UpdateEffect::applied);
            const bool takes_trace = type != 1 && type != 5 && fraction < 0.97f;
            CHECK(same_position(record.position,
                                takes_trace ? observed_trace.end_position : input.position));
            CHECK(record.flags == (fraction <= 0.5f ? 0u : 0xf5u));
        }
    }
    auto record = seed();
    record.sequence = std::numeric_limits<std::int32_t>::min();
    auto signed_update = incoming();
    signed_update.sequence = std::numeric_limits<std::int32_t>::max();
    CHECK(update_resolved_record(record, signed_update, trace()) == UpdateEffect::applied);
    signed_update.sequence = std::numeric_limits<std::int32_t>::min();
    const auto before = record;
    CHECK(update_resolved_record(record, signed_update, trace()) == UpdateEffect::older_type_two_sequence);
    CHECK(same_bytes(record, before));
}

void reader_policy()
{
    PositionStore store;
    auto input = reading();
    input.slot_flag = false;
    input.slot_value = std::numeric_limits<std::int32_t>::max();
    for (bool primary : {false, true}) {
        for (bool alternate : {false, true}) {
            for (std::uint8_t selector : {0, 1, 2, 255}) {
                for (float marker : {0.0f, -0.0f, nan, -1.0f, 1.0f, infinity}) {
                    auto& record = store.at(64);
                    record = seed();
                    record.active = 0x80;
                    record.validity_marker = marker;
                    input.options = {primary, alternate, selector};
                    const auto before = record;
                    const auto result = read_resolved_record(store, 64, input);
                    const bool enabled = selector ? alternate : primary;
                    const bool clears = !enabled || marker == 0.0f || std::isnan(marker);
                    CHECK(result.record == &record);
                    CHECK(result.status == (clears ? ReadStatus::cleared_by_policy : ReadStatus::usable));
                    auto expected = before;
                    if (clears) expected.active = 0;
                    CHECK(same_bytes(record, expected));
                }
            }
        }
    }
    for (std::uint32_t index : {0u, 64u, 65u, 2047u}) {
        for (bool flag : {false, true}) {
            for (std::int32_t value : {-1, 0, 1}) {
                auto& record = store.at(index);
                record = seed();
                record.active = 1;
                input = reading();
                input.slot_flag = flag;
                input.slot_value = value;
                input.options = {false, false, 0};
                const auto result = read_resolved_record(store, index, input);
                const bool bypasses_policy = index > 64 || (flag && value > 0);
                CHECK(result.status == (bypasses_policy ? ReadStatus::usable : ReadStatus::cleared_by_policy));
                CHECK(record.active == (bypasses_policy ? 1 : 0));
            }
        }
    }
}

void reader_boundaries_and_borrowing()
{
    PositionStore store;
    auto& record = store.at(1);
    record = seed();
    record.active = 1;
    auto input = reading();
    input.source_available = false;
    input.options = {false, false, 0};
    const auto before = record;
    const auto unavailable = read_resolved_record(store, 1, input);
    CHECK(unavailable.record == &record && unavailable.status == ReadStatus::source_unavailable);
    CHECK(same_bytes(record, before));
    CHECK(!passes_item_consumer_gate(unavailable));
    for (auto index : {2048u, std::numeric_limits<std::uint32_t>::max()}) {
        const auto invalid = read_resolved_record(store, index, input);
        CHECK(invalid.record == nullptr && invalid.status == ReadStatus::outside_supported_domain);
        CHECK(!passes_item_consumer_gate(invalid));
    }
    input = reading();
    record.updated_at = 0.0f;
    for (float now : {-infinity, -1.0f, 0.0f, std::nextafter(4.0f, 0.0f), 4.0f, infinity, nan}) {
        input.now = now;
        const auto prior = record;
        const auto result = read_resolved_record(store, 1, input);
        CHECK(result.status == ((now < 4.0f || std::isnan(now)) ? ReadStatus::usable : ReadStatus::expired));
        CHECK(same_bytes(record, prior));
    }
    input.now = infinity;
    record.updated_at = infinity;
    CHECK(read_resolved_record(store, 1, input).status == ReadStatus::usable);
    record.active = 0;
    input.now = nan;
    CHECK(read_resolved_record(store, 1, input).status == ReadStatus::inactive);
}

void metadata_and_connected_chain()
{
    PositionStore store;
    auto& record = store.at(17);
    auto input = incoming();
    input.type = 1; // Observed packet update type, not an invented full packet decoder.
    CHECK(update_resolved_record(record, input, trace()) == UpdateEffect::applied);
    CHECK(same_position(record.position, input.position));
    auto read = read_resolved_record(store, 17, reading());
    CHECK(read.record == &record && read.status == ReadStatus::usable);
    CHECK(!passes_item_consumer_gate(read));
    for (auto word : {0u, 7u, 0x7fffu, 0x8000u, 0xffffu}) {
        const auto before = record;
        store_resolved_item_definition(record, static_cast<std::uint16_t>(word));
        const auto value = word < 0x8000u ? static_cast<std::int32_t>(word)
                                         : static_cast<std::int32_t>(word) - 0x10000;
        auto expected = before;
        expected.item_definition = value;
        CHECK(same_bytes(record, expected));
        CHECK(passes_item_consumer_gate(read) == (word != 0));
        store_resolved_item_definition(record, std::nullopt);
        CHECK(same_bytes(record, expected));
    }
    auto old = reading();
    old.now = input.now + 4.0f;
    read = read_resolved_record(store, 17, old);
    CHECK(read.record == &record && read.status == ReadStatus::expired);
    CHECK(record.item_definition == -1 && !passes_item_consumer_gate(read));
}
} // namespace

int main()
{
    CHECK(std::fegetround() == FE_TONEAREST);
    initialization();
    update_lifecycle();
    rejected_updates_do_not_write();
    snapshot_rules();
    numerical_boundaries();
    reader_policy();
    reader_boundaries_and_borrowing();
    metadata_and_connected_chain();
    std::cout << "PASS: 8 fixture groups, " << assertions
              << " assertions; offline conditional contracts, not full DLL equivalence\n";
}

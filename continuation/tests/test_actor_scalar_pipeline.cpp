#include "rage/actor_scalar_pipeline.hpp"
#include "rage/position_lifecycle.hpp"

#include <array>
#include <cstdint>
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

float from_bits(std::uint32_t bits)
{
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool same_vector(const Vec3& left, const Vec3& right)
{
    return std::memcmp(&left, &right, sizeof(left)) == 0;
}

PositionRecord record_with(const Vec3& current, const Vec3& target)
{
    PositionRecord record{};
    record.active = 0x80;
    record.updated_at = 10.0f;
    record.position = target;
    record.actor_position = current;
    record.validity_marker = 0.75f;
    record.opaque_10.fill(0xa5);
    record.opaque_4c.fill(0x5a);
    const float side50 = 0.25f;
    std::memcpy(reinterpret_cast<unsigned char*>(&record) + 0x50,
                &side50, sizeof(side50));
    return record;
}

ActorScalarObservations half_steps()
{
    const float half_divisor = from_bits(0x3D2E147B);
    return {{0.0f, half_divisor}, {0.0f, half_divisor}, {0.0f, half_divisor}};
}

Vec3 resolved(const PositionRecord& record, ActorScalarObservations observations)
{
    std::array<unsigned char, sizeof(PositionRecord)> before{};
    std::memcpy(before.data(), &record, sizeof(record));
    const auto result = resolve_actor_scalar_position(record, observations);
    CHECK(std::memcmp(before.data(), &record, sizeof(record)) == 0);
    CHECK(std::holds_alternative<Vec3>(result));
    return std::get<Vec3>(result);
}

void independent_observations()
{
    const auto record = record_with({2.0f, -4.0f, 6.0f}, {10.0f, 12.0f, -2.0f});
    const ActorScalarObservations observations{
        {0.0f, from_bits(0x3CAE147B)},
        {0.5f, from_bits(0x3D2E147B)},
        {-1.0f, from_bits(0x3DAE147B)}};
    CHECK(same_vector(resolved(record, observations), Vec3{4.0f, 4.0f, -2.0f}));

    const auto thresholds = record_with({0.0f, 0.0f, 0.0f}, {2.0f, 4.0f, 8.0f});
    auto snap = half_steps();
    snap.x.tolerance = 2.0f;
    snap.y.tolerance = 3.0f;
    snap.z.tolerance = from_bits(0x7fc12345);
    CHECK(same_vector(resolved(thresholds, snap), Vec3{2.0f, 2.0f, 8.0f}));
}

void equality_unordered_and_signed_zero()
{
    const float nan_x = from_bits(0x7fc12345);
    const float nan_y = from_bits(0xffc54321);
    const float nan_z = from_bits(0x7fcabcde);
    const float infinity = std::numeric_limits<float>::infinity();
    struct Case { Vec3 current; Vec3 target; };
    const std::array<Case, 4> cases{{
        {{nan_x, -0.0f, infinity}, {1.0f, 0.0f, infinity}},
        {{0.0f, nan_y, -0.0f}, {-0.0f, 1.0f, 0.0f}},
        {{1.0f, 2.0f, nan_z}, {nan_x, nan_y, 3.0f}},
        {{5.0f, -infinity, 7.0f}, {5.0f, -infinity, nan_z}},
    }};
    const ActorScalarObservations absent{
        {nan_x, std::nullopt}, {-1.0f, std::nullopt}, {infinity, std::nullopt}};
    for (const auto& test : cases) {
        const auto record = record_with(test.current, test.target);
        CHECK(same_vector(resolved(record, absent), test.current));
    }

    auto zero_steps = half_steps();
    zero_steps.x = {-1.0f, 0.0f};
    zero_steps.y = {-1.0f, -0.0f};
    zero_steps.z = {-1.0f, 0.0f};
    const auto zero_record = record_with({-0.0f, -0.0f, 0.0f}, {-1.0f, 1.0f, -1.0f});
    CHECK(same_vector(resolved(zero_record, zero_steps), Vec3{-0.0f, -0.0f, 0.0f}));

    const auto snap_record = record_with({1.0f, -1.0f, 1.0f}, {-0.0f, 0.0f, -0.0f});
    const ActorScalarObservations snap{
        {1.0f, nan_x}, {1.0f, nan_y}, {nan_z, nan_z}};
    CHECK(same_vector(resolved(snap_record, snap), snap_record.position));
}

void missing_fallback_per_axis()
{
    const auto record = record_with({0.0f, 0.0f, 0.0f}, {2.0f, 4.0f, 8.0f});
    const std::array<ActorScalarAxis, 3> axes{{
        ActorScalarAxis::x, ActorScalarAxis::y, ActorScalarAxis::z}};
    for (std::size_t axis = 0; axis < axes.size(); ++axis) {
        for (float tolerance : {0.0f, std::numeric_limits<float>::infinity(),
                                from_bits(0x7fc12345)}) {
            auto observations = half_steps();
            const std::array<ActorScalarAxisObservation*, 3> values{{
                &observations.x, &observations.y, &observations.z}};
            *values[axis] = {tolerance, std::nullopt};
            const auto before = record;
            const auto result = resolve_actor_scalar_position(record, observations);
            CHECK(std::holds_alternative<ActorScalarUnresolved>(result));
            CHECK(std::get<ActorScalarUnresolved>(result).axis == axes[axis]);
            CHECK(std::memcmp(&record, &before, sizeof(record)) == 0);
        }
    }

    const ActorScalarObservations absent{
        {0.0f, std::nullopt}, {0.0f, std::nullopt}, {0.0f, std::nullopt}};
    const auto first = resolve_actor_scalar_position(record, absent);
    CHECK(std::get<ActorScalarUnresolved>(first).axis == ActorScalarAxis::x);
    const auto skip_x = record_with({2.0f, 0.0f, 0.0f}, record.position);
    const auto second = resolve_actor_scalar_position(skip_x, absent);
    CHECK(std::get<ActorScalarUnresolved>(second).axis == ActorScalarAxis::y);
    const auto skip_xy = record_with({2.0f, 4.0f, 0.0f}, record.position);
    const auto third = resolve_actor_scalar_position(skip_xy, absent);
    CHECK(std::get<ActorScalarUnresolved>(third).axis == ActorScalarAxis::z);
}

void bounds_and_unordered_candidate()
{
    const float bound = std::numeric_limits<float>::max();
    const float infinity = std::numeric_limits<float>::infinity();
    const auto record = record_with({0.0f, 0.0f, 0.0f}, {1.0f, -1.0f, 1.0f});
    const ActorScalarObservations observations{
        {0.0f, infinity}, {0.0f, infinity}, {0.0f, from_bits(0x7fc12345)}};
    CHECK(same_vector(resolved(record, observations), Vec3{bound, -bound, -bound}));

    const auto overflow = record_with({-bound, bound, 0.0f}, {bound, -bound, 1.0f});
    auto steps = half_steps();
    steps.z.fallback_step = -from_bits(0x3DAE147B);
    CHECK(same_vector(resolved(overflow, steps), Vec3{bound, -bound, -1.0f}));
}

void coherent_consumer_composition()
{
    PositionStore store;
    auto& record = store.at(17);
    record = record_with({2.0f, 4.0f, 6.0f}, {10.0f, 12.0f, 14.0f});
    const auto read = read_resolved_record(store, 17,
        {true, false, 0, {true, false, 0}, 10.0f});
    CHECK(read.status == ReadStatus::usable);
    CHECK(read.record == &record);
    const auto before = record;
    Vec3 sink{90.0f, 91.0f, 92.0f};
    float marker = -77.0f;
    float side50 = -88.0f;
    const ActorScalarObservations observations{
        {0.1f, from_bits(0x3D2E147B)},
        {0.2f, from_bits(0x3CAE147B)},
        {0.3f, from_bits(0x3DAE147B)}};
    const auto result = resolve_actor_scalar_position(record, observations);
    CHECK(std::holds_alternative<Vec3>(result));
    CHECK(same_vector(std::get<Vec3>(result), Vec3{6.0f, 6.0f, 14.0f}));
    CHECK(std::memcmp(&record, &before, sizeof(record)) == 0);
    CHECK(same_vector(sink, Vec3{90.0f, 91.0f, 92.0f}));
    CHECK(marker == -77.0f && side50 == -88.0f);

    int owner = 0;
    const auto consumed = update_actor_consumer({&owner, read, sink},
        {true, ActorGuardOutcome::passed, true, std::get<Vec3>(result), &marker,
         {&side50, false, 10.0f}});
    auto expected = before;
    expected.actor_position = std::get<Vec3>(result);
    CHECK(consumed.transition == ActorTransition::resolved_position_applied);
    CHECK(consumed.actor_owner == &owner && consumed.record == &record);
    CHECK(consumed.actor_position_sink == &sink);
    CHECK(same_vector(sink, expected.actor_position));
    CHECK(std::memcmp(&record, &expected, sizeof(record)) == 0);
    CHECK(marker == record.validity_marker && side50 == 1.0f);

    record = before;
    auto missing = observations;
    missing.z.fallback_step = std::nullopt;
    const auto unresolved = resolve_actor_scalar_position(record, missing);
    CHECK(std::get<ActorScalarUnresolved>(unresolved).axis == ActorScalarAxis::z);
    CHECK(std::memcmp(&record, &before, sizeof(record)) == 0);
    const Vec3 old_sink = sink;
    side50 = -88.0f;
    const auto refused = update_actor_consumer({&owner, read, sink},
        {true, ActorGuardOutcome::passed, true, std::nullopt, &marker,
         {&side50, false, 10.0f}});
    CHECK(refused.transition == ActorTransition::provider_observation_missing);
    CHECK(std::memcmp(&record, &before, sizeof(record)) == 0);
    CHECK(same_vector(sink, old_sink));
    CHECK(side50 == -88.0f && marker == record.validity_marker);
}

void consumer_bypasses_without_observations()
{
    struct Case {
        bool has_record;
        ActorGuardOutcome guard;
        bool active;
        bool al;
        float marker;
        bool adjustment;
        float current_x;
        ActorTransition transition;
        bool copies_position;
        bool clears_marker;
    };
    const std::array<Case, 8> cases{{
        {false, ActorGuardOutcome::passed, true, true, 0.75f, true, 1.0f,
         ActorTransition::no_record, false, true},
        {true, ActorGuardOutcome::exit_copy_marker, true, true, 0.75f, true, 1.0f,
         ActorTransition::guard_exit_copy_marker, false, false},
        {true, ActorGuardOutcome::exit_clear_marker, true, true, 0.75f, true, 1.0f,
         ActorTransition::guard_exit_clear_marker, false, true},
        {true, ActorGuardOutcome::passed, false, true, 0.75f, true, 1.0f,
         ActorTransition::inactive_record, false, false},
        {true, ActorGuardOutcome::passed, true, false, -0.0f, true, 1.0f,
         ActorTransition::marker_gate_rejected, false, false},
        {true, ActorGuardOutcome::passed, true, true, 0.75f, false, 1.0f,
         ActorTransition::copied_record_position, true, false},
        {true, ActorGuardOutcome::passed, true, true, 0.75f, true, 100.0f,
         ActorTransition::copied_record_position, true, false},
        {true, ActorGuardOutcome::passed, true, true, 0.75f, true, 101.0f,
         ActorTransition::copied_record_position, true, false},
    }};
    for (const auto& test : cases) {
        auto record = record_with({test.current_x, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        record.active = test.active ? 0x80 : 0;
        record.validity_marker = test.marker;
        auto expected = record;
        Vec3 sink{90.0f, 91.0f, 92.0f};
        const Vec3 expected_sink = test.copies_position ? record.position : sink;
        if (test.copies_position)
            expected.actor_position = record.position;
        const PositionRead read{test.has_record ? &record : nullptr, ReadStatus::usable};
        float marker = -77.0f;
        float side50 = 0.5f;
        int owner = 0;
        // Intentionally do not resolve axes on branches that never use them.
        const auto result = update_actor_consumer({&owner, read, sink},
            {test.al, test.guard, test.adjustment, std::nullopt, &marker,
             {&side50, false, 10.0f}});
        CHECK(result.transition == test.transition);
        CHECK(std::memcmp(&record, &expected, sizeof(record)) == 0);
        CHECK(same_vector(sink, expected_sink));
        const float expected_marker = test.clears_marker ? 0.0f : test.marker;
        CHECK(std::memcmp(&marker, &expected_marker, sizeof(marker)) == 0);
        CHECK(side50 == (test.copies_position ? 1.0f : 0.5f));
    }
}
} // namespace

int main()
{
    independent_observations();
    equality_unordered_and_signed_zero();
    missing_fallback_per_axis();
    bounds_and_unordered_candidate();
    coherent_consumer_composition();
    consumer_bypasses_without_observations();
    std::cout << "PASS actor_scalar_pipeline: " << assertions << " assertions\n";
}

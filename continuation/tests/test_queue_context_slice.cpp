#include "rage/queue_context_slice.hpp"

#include <array>
#include <cstddef>
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

void put(std::uint8_t* bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}

std::uint32_t get(const std::uint8_t* bytes, std::size_t offset)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8 * i);
    return value;
}

bool capture_preparation_and_consumer()
{
    using namespace rage::offline;
    auto capture = capture_queue_context(0x0012ff00u);
    CHECK(capture.address == 0x0012fc64u);
    const QueueContextBytes expected{{
        0x10, 0xd9, 0x4a, 0x41, 0x64, 0xfe, 0x12, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    CHECK(capture.bytes == expected);
    const auto prepared = prepare_queue_range(capture.bytes, 0x4223908cu, 128);
    CHECK(prepared.exchanged_cursor == 0);
    CHECK(prepared.end == 0x4223928cu);
    CHECK(get(capture.bytes.data(), 0) == 0x414ad910u);
    CHECK(get(capture.bytes.data(), 4) == 0x0012fe64u);
    CHECK(get(capture.bytes.data(), 8) == 0x4223908cu);
    CHECK(get(capture.bytes.data(), 12) == 0x4223928cu);

    QueueTaskBytes task;
    task.fill(0xa5);
    prepare_queue_task(task, capture.address);
    CHECK(get(task.data(), 0) == 0x42078104u);
    CHECK(get(task.data(), 4) == 0xa5a5a5a5u);
    CHECK(get(task.data(), 8) == 0x420780f0u);
    CHECK(get(task.data(), 12) == 1);
    CHECK(get(task.data(), 16) == 0x414ac2d0u);
    CHECK(get(task.data(), 20) == capture.address);
    const auto task_entry = observe_queue_task_entry(task);
    CHECK(task_entry.target == 0x414ac2d0u);
    CHECK(task_entry.ecx == capture.address);

    const auto entry = observe_queue_worker_entry(capture.bytes);
    CHECK(entry.cursor == 0x4223908cu);
    CHECK(entry.captured_end == 0x4223928cu);
    CHECK(entry.continuation == QueueWorkerContinuation::claim_required);
    const auto claim = project_queue_worker_claim(entry.captured_end, entry.cursor);
    CHECK(claim.cursor_written_by_xadd == 0x42239090u);
    CHECK(claim.chunk_end == 0x42239090u);
    CHECK(claim.callback_slot_address == entry.cursor);
    CHECK(claim.continuation_after_normal_callback == QueueWorkerContinuation::claim_required);
    const auto call = observe_queue_callback(capture.bytes, *claim.callback_slot_address);
    CHECK(call.target == 0x414ad910u);
    CHECK(call.argument_0_slot_address == 0x4223908cu);
    CHECK(call.argument_1_context_address == 0x0012fe64u);
    CHECK(get(capture.bytes.data(), 8) == entry.cursor);
    return true;
}

bool unsigned_range_and_entry_edges()
{
    using namespace rage::offline;
    QueueContextBytes context;
    context.fill(0x81);
    const auto before = context;
    auto prepared = prepare_queue_range(context, 0xfffffffcu, 2);
    CHECK(prepared.exchanged_cursor == 0x81818181u);
    CHECK(prepared.end == 4);
    for (std::size_t i = 0; i < 8; ++i)
        CHECK(context[i] == before[i]);
    CHECK(observe_queue_worker_entry(context).continuation
          == QueueWorkerContinuation::return_from_worker);
    prepared = prepare_queue_range(context, 0x1000u, 0x40000000u);
    CHECK(prepared.exchanged_cursor == 0xfffffffcu);
    CHECK(prepared.end == 0x1000u);
    CHECK(observe_queue_worker_entry(context).continuation
          == QueueWorkerContinuation::return_from_worker);
    prepared = prepare_queue_range(context, 0, 0);
    CHECK(prepared.end == 0);
    CHECK(observe_queue_worker_entry(context).continuation
          == QueueWorkerContinuation::return_from_worker);
    put(context.data(), 8, 0x7ffffffcu);
    put(context.data(), 12, 0x80000000u);
    CHECK(observe_queue_worker_entry(context).continuation
          == QueueWorkerContinuation::claim_required);
    CHECK(capture_queue_context(0).address == 0xfffffd64u);
    CHECK(get(capture_queue_context(0).bytes.data(), 4) == 0xffffff64u);
    return true;
}

bool claim_boundaries()
{
    using namespace rage::offline;
    struct Case {
        std::uint32_t end, old, written, chunk;
        bool callback, claim_again;
    };
    const std::array<Case, 11> cases{{
        {0x1008, 0x1000, 0x1004, 0x1004, true, true},
        {0x1008, 0x1004, 0x1008, 0x1008, true, false},
        {0x1008, 0x1008, 0x100c, 0x1008, false, false},
        {0x1008, 0x100c, 0x1010, 0x1008, false, false},
        {4, 1, 5, 4, true, false},
        {0x80000000, 0x7ffffffc, 0x80000000, 0x80000000, true, false},
        {0xffffffff, 0xfffffffb, 0xffffffff, 0xffffffff, true, false},
        {0xffffffff, 0xfffffffc, 0, 0, false, true},
        {0xffffffff, 0xffffffff, 3, 3, false, false},
        {0, 0xfffffffc, 0, 0, false, false},
        {0, 0, 4, 0, false, false},
    }};
    for (const auto& c : cases) {
        const auto p = project_queue_worker_claim(c.end, c.old);
        CHECK(p.cursor_written_by_xadd == c.written);
        CHECK(p.chunk_end == c.chunk);
        CHECK(p.callback_slot_address.has_value() == c.callback);
        if (c.callback)
            CHECK(*p.callback_slot_address == c.old);
        CHECK((p.continuation_after_normal_callback == QueueWorkerContinuation::claim_required)
              == c.claim_again);
    }
    return true;
}

bool observations_are_not_a_scheduler_or_callback_provider()
{
    using namespace rage::offline;
    auto capture = capture_queue_context(0x12340000u);
    prepare_queue_range(capture.bytes, 0x1000u, 2);
    const auto entry = observe_queue_worker_entry(capture.bytes);
    const auto independent_claim = project_queue_worker_claim(entry.captured_end, 0x1008u);
    CHECK(!independent_claim.callback_slot_address);
    CHECK(independent_claim.cursor_written_by_xadd == 0x100cu);
    CHECK(independent_claim.continuation_after_normal_callback
          == QueueWorkerContinuation::return_from_worker);

    const auto claim = project_queue_worker_claim(entry.captured_end, 0x1000u);
    put(capture.bytes.data(), 0, 0x87654321u);
    put(capture.bytes.data(), 4, 0xfedcba98u);
    put(capture.bytes.data(), 12, 0);
    const auto before = capture.bytes;
    const auto call = observe_queue_callback(capture.bytes, *claim.callback_slot_address);
    CHECK(call.target == 0x87654321u);
    CHECK(call.argument_0_slot_address == 0x1000u);
    CHECK(call.argument_1_context_address == 0xfedcba98u);
    CHECK(capture.bytes == before);
    CHECK(claim.continuation_after_normal_callback == QueueWorkerContinuation::claim_required);
    CHECK(observe_queue_worker_entry(capture.bytes).continuation
          == QueueWorkerContinuation::return_from_worker);

    QueueTaskBytes task{};
    put(task.data(), 0x10, 0);
    put(task.data(), 0x14, 0xffffffffu);
    CHECK(observe_queue_task_entry(task).target == 0);
    CHECK(observe_queue_task_entry(task).ecx == 0xffffffffu);
    return true;
}

} // namespace

int main()
{
    if (!capture_preparation_and_consumer() || !unsigned_range_and_entry_edges()
        || !claim_boundaries() || !observations_are_not_a_scheduler_or_callback_provider())
        return 1;
    std::cout << "PASS: raw queue capture, task preparation and observed worker claim ("
              << assertions << " assertions)\n";
    return 0;
}

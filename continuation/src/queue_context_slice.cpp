#include "rage/queue_context_slice.hpp"

#include <cstddef>

namespace rage::offline {
namespace {

std::uint32_t load_word(const std::uint8_t* bytes, std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

void store_word(std::uint8_t* bytes, std::size_t offset, std::uint32_t word) noexcept
{
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(word >> (i * 8));
}

QueueWorkerContinuation after_compare(std::uint32_t cursor, std::uint32_t end) noexcept
{
    return cursor < end ? QueueWorkerContinuation::claim_required
                        : QueueWorkerContinuation::return_from_worker;
}

} // namespace

QueueContextCapture capture_queue_context(std::uint32_t caller_ebp) noexcept
{
    QueueContextCapture capture{caller_ebp - 0x29cu, {}};
    store_word(capture.bytes.data(), 0x08, 0);
    store_word(capture.bytes.data(), 0x0c, 0);
    store_word(capture.bytes.data(), 0x08, 0);
    store_word(capture.bytes.data(), 0x00, 0x414ad910u);
    store_word(capture.bytes.data(), 0x04, caller_ebp - 0x9cu);
    return capture;
}

QueueRangePreparation prepare_queue_range(
    QueueContextBytes& context, std::uint32_t begin, std::uint32_t count) noexcept
{
    const std::uint32_t end = begin + count * 4u;
    const std::uint32_t old_cursor = load_word(context.data(), 0x08);
    store_word(context.data(), 0x08, begin);
    store_word(context.data(), 0x0c, end);
    return {old_cursor, end};
}

void prepare_queue_task(QueueTaskBytes& task, std::uint32_t context_address) noexcept
{
    store_word(task.data(), 0x00, 0x42078104u);
    store_word(task.data(), 0x08, 0x420780f0u);
    store_word(task.data(), 0x0c, 1);
    store_word(task.data(), 0x10, 0x414ac2d0u);
    store_word(task.data(), 0x14, context_address);
}

QueueTaskEntry observe_queue_task_entry(const QueueTaskBytes& task) noexcept
{
    const std::uint32_t ecx = load_word(task.data(), 0x14);
    return {load_word(task.data(), 0x10), ecx};
}

QueueWorkerEntry observe_queue_worker_entry(const QueueContextBytes& context) noexcept
{
    const std::uint32_t cursor = load_word(context.data(), 0x08);
    const std::uint32_t end = load_word(context.data(), 0x0c);
    return {cursor, end, after_compare(cursor, end)};
}

QueueClaimProjection project_queue_worker_claim(
    std::uint32_t captured_end, std::uint32_t xadd_old_cursor) noexcept
{
    const std::uint32_t next = xadd_old_cursor + 4u;
    const std::uint32_t chunk_end = next >= captured_end ? captured_end : next;
    const bool calls_callback = chunk_end > xadd_old_cursor;
    // The inner loop has at most one call: chunk_end <= old+4 without wrap.
    const std::uint32_t compare_cursor = calls_callback ? next : xadd_old_cursor;
    return {next, chunk_end,
            calls_callback ? std::optional<std::uint32_t>{xadd_old_cursor} : std::nullopt,
            after_compare(compare_cursor, captured_end)};
}

QueueCallbackInvocation observe_queue_callback(
    const QueueContextBytes& callback_load_snapshot, std::uint32_t slot_address) noexcept
{
    const std::uint32_t argument_1 = load_word(callback_load_snapshot.data(), 0x04);
    return {load_word(callback_load_snapshot.data(), 0x00), slot_address, argument_1};
}

} // namespace rage::offline

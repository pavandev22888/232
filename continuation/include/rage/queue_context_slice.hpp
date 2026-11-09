#ifndef RAGE_QUEUE_CONTEXT_SLICE_HPP
#define RAGE_QUEUE_CONTEXT_SLICE_HPP

#include <array>
#include <cstdint>
#include <optional>

namespace rage::offline {

using QueueContextBytes = std::array<std::uint8_t, 0x10>;
using QueueTaskBytes = std::array<std::uint8_t, 0x18>;

struct QueueContextCapture {
    std::uint32_t address;
    QueueContextBytes bytes;
};

// 0x4148FDA1..0x4148FDD3: raw 32-bit stack addresses, never host pointers.
QueueContextCapture capture_queue_context(std::uint32_t caller_ebp) noexcept;

struct QueueRangePreparation {
    std::uint32_t exchanged_cursor;
    std::uint32_t end;
};

// Sequential snapshot of XCHG +8, then store +C; no synchronization/provider.
QueueRangePreparation prepare_queue_range(
    QueueContextBytes& context, std::uint32_t begin, std::uint32_t count) noexcept;

// Requires an observed successful writable allocation; leaves +4 untouched.
void prepare_queue_task(QueueTaskBytes& task, std::uint32_t context_address) noexcept;

struct QueueTaskEntry {
    std::uint32_t target;
    std::uint32_t ecx;
};

// 0x4149BFC0: load +14 into ECX, jump through +10, without executing either.
QueueTaskEntry observe_queue_task_entry(const QueueTaskBytes& task) noexcept;

enum class QueueWorkerContinuation {
    claim_required,
    return_from_worker
};

struct QueueWorkerEntry {
    std::uint32_t cursor;
    std::uint32_t captured_end;
    QueueWorkerContinuation continuation;
};

QueueWorkerEntry observe_queue_worker_entry(const QueueContextBytes& context) noexcept;

struct QueueClaimProjection {
    std::uint32_t cursor_written_by_xadd;
    std::uint32_t chunk_end;
    std::optional<std::uint32_t> callback_slot_address;
    QueueWorkerContinuation continuation_after_normal_callback;
};

// One 0x414AC2F9 claim, given an independently observed old XADD value.
// Does not assume it equals the entry cursor, mutate shared memory, or schedule.
// With no callback, continuation applies immediately; otherwise only on return.
QueueClaimProjection project_queue_worker_claim(
    std::uint32_t captured_end, std::uint32_t xadd_old_cursor) noexcept;

struct QueueCallbackInvocation {
    std::uint32_t target;
    std::uint32_t argument_0_slot_address;
    std::uint32_t argument_1_context_address;
};

// Separate +4/+0 observations at 0x414AC310..314, after XADD; no dereference.
// Snapshots must represent the loads with stable accessible storage. Original
// callback execution, enqueue/wait providers and stack lifetime stay unresolved.
QueueCallbackInvocation observe_queue_callback(
    const QueueContextBytes& callback_load_snapshot, std::uint32_t slot_address) noexcept;

} // namespace rage::offline
#endif

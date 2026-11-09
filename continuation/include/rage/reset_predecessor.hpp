#ifndef RAGE_RESET_PREDECESSOR_HPP
#define RAGE_RESET_PREDECESSOR_HPP

#include "rage/charge_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rage::offline {

enum class ResetPredecessorBranch {
    repeat_loop,
    enter_reset_block,
};

inline constexpr std::size_t kResetBodyStoreCount = 25;

struct ResetFallthroughSnapshot {
    std::uint32_t dword_424fdda8{};
    std::uint32_t dword_424f4b7c{};
    std::uint32_t dword_425008cc{};
    std::uint8_t byte_425009e9{};
};

struct ResetPredecessorSnapshot {
    ChargeResetSnapshot charge_reset;
    ResetFallthroughSnapshot fallthrough;
};

struct ResetBodyDwordStore {
    std::uint32_t offset;
    std::uint32_t value;
};

enum class ResetStoreWidth : std::uint8_t {
    byte = 1,
    dword = 4,
};

struct ResetFallthroughStore {
    std::uint32_t address;
    std::uint32_t value;
    ResetStoreWidth width;
};

struct ResetPredecessorRecord {
    std::uint32_t eax_base{};
    std::array<ResetBodyDwordStore, kResetBodyStoreCount> body_stores{};
    std::array<ResetFallthroughStore, 4> fallthrough_stores{};
    std::size_t fallthrough_store_count{};
};

// Models the local add/cmp/JNE and 11-store projection; EAX provenance and
// the loop's table writes are not modeled.
ResetPredecessorBranch apply_reset_predecessor_projection(
    std::uint32_t eax_before_add,
    ChargeResetSnapshot& snapshot);

// Records one source-backed table-body iteration and the local fallthrough
// stores. EAX is caller-supplied; this does not infer its provider or entry.
ResetPredecessorBranch apply_reset_predecessor_projection(
    std::uint32_t eax_before_add,
    ResetPredecessorSnapshot& snapshot,
    ResetPredecessorRecord& record);

} // namespace rage::offline

#endif

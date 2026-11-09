#ifndef RAGE_RESET_PREDECESSOR_HPP
#define RAGE_RESET_PREDECESSOR_HPP

#include "rage/charge_state.hpp"

#include <cstdint>

namespace rage::offline {

enum class ResetPredecessorBranch {
    repeat_loop,
    enter_reset_block,
};

// Models the local add/cmp/JNE and 11-store projection; EAX provenance and
// the loop's table writes are not modeled.
ResetPredecessorBranch apply_reset_predecessor_projection(
    std::uint32_t eax_before_add,
    ChargeResetSnapshot& snapshot);

} // namespace rage::offline

#endif

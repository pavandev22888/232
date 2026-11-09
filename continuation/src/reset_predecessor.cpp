#include "rage/reset_predecessor.hpp"

namespace rage::offline {
namespace {
constexpr std::uint32_t kCursorStride = 0xc8u;
constexpr std::uint32_t kCursorEnd = 0x4250309cu;
} // namespace

ResetPredecessorBranch apply_reset_predecessor_projection(
    std::uint32_t eax_before_add,
    ChargeResetSnapshot& snapshot)
{
    const auto eax_after_add = eax_before_add + kCursorStride;
    if (eax_after_add != kCursorEnd)
        return ResetPredecessorBranch::repeat_loop;

    apply_full_local_charge_reset(snapshot);
    return ResetPredecessorBranch::enter_reset_block;
}

} // namespace rage::offline

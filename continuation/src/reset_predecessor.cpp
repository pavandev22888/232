#include "rage/reset_predecessor.hpp"

namespace rage::offline {
namespace {
constexpr std::uint32_t kCursorStride = 0xc8u;
constexpr std::uint32_t kCursorEnd = 0x4250309cu;

constexpr std::array<ResetFallthroughStore, 4> kFallthroughStores{{
    {0x424fdda8u, 0xffffffffu, ResetStoreWidth::dword},
    {0x424f4b7cu, 0x00000000u, ResetStoreWidth::dword},
    {0x425008ccu, 0xffffffffu, ResetStoreWidth::dword},
    {0x425009e9u, 0x00000000u, ResetStoreWidth::byte},
}};

constexpr bool enters_reset_block(std::uint32_t eax_before_add)
{
    return eax_before_add + kCursorStride == kCursorEnd;
}
} // namespace

ResetPredecessorBranch apply_reset_predecessor_projection(
    std::uint32_t eax_before_add,
    ChargeResetSnapshot& snapshot)
{
    if (!enters_reset_block(eax_before_add))
        return ResetPredecessorBranch::repeat_loop;

    apply_full_local_charge_reset(snapshot);
    return ResetPredecessorBranch::enter_reset_block;
}

ResetPredecessorBranch apply_reset_predecessor_projection(
    std::uint32_t eax_before_add,
    ResetPredecessorSnapshot& snapshot,
    ResetPredecessorRecord& record)
{
    record = {};
    record.eax_base = eax_before_add;
    for (std::size_t index = 0; index < record.body_stores.size(); ++index) {
        record.body_stores[index] = {
            static_cast<std::uint32_t>(index * 8u), 0xffffffffu};
    }

    if (!enters_reset_block(eax_before_add))
        return ResetPredecessorBranch::repeat_loop;

    record.fallthrough_stores = kFallthroughStores;
    record.fallthrough_store_count = kFallthroughStores.size();
    snapshot.fallthrough = {0xffffffffu, 0u, 0xffffffffu, 0u};
    apply_full_local_charge_reset(snapshot.charge_reset);
    return ResetPredecessorBranch::enter_reset_block;
}

} // namespace rage::offline

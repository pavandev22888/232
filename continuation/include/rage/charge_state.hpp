#ifndef RAGE_CHARGE_STATE_HPP
#define RAGE_CHARGE_STATE_HPP

#include <cstdint>

namespace rage::offline {

struct ChargeCapacityInput {
    std::int32_t raw_limit;
    std::int32_t configured_count;
    bool use_allowed_limit;
    std::int32_t option;
};

struct ChargeCapacity {
    std::int32_t cap{};
    std::int32_t release_limit{};
    std::int32_t allowed{};
    std::int32_t selected{};
};

ChargeCapacity calculate_charge_capacity(const ChargeCapacityInput& input,
                                         std::uint8_t raw_mode_result);

struct SmallGateInput {
    bool guard_a;
    bool guard_c;
    bool enabled_d;
    std::uint32_t current_counter;
    std::uint32_t previous_counter;
};

bool evaluate_small_charge_gate(const SmallGateInput& input,
                                bool mode_latched);

struct ChargeObservation {
    std::uint8_t raw_mode_result;
    ChargeCapacityInput capacity;
    SmallGateInput gate;
};

struct ChargeState {
    bool mode_latched{};
    ChargeCapacity capacity{};
    bool small_gate_result{};
};

// Projects the saved reset block onto the latch and the same-global gate
// snapshot only. Entry scheduling is not modeled.
void apply_charge_reset_slice(ChargeState& state, SmallGateInput& gate);

// Raw values at the eleven absolute destinations, not an original memory layout.
struct ChargeResetSnapshot {
    std::uint8_t byte_425009bc{};
    std::uint8_t byte_4223f42c{};
    std::uint32_t dword_424b1c74{};
    std::uint32_t dword_42502ba0{};
    std::uint32_t dword_425057f8{};
    std::uint32_t dword_4250097c{};
    std::uint32_t dword_42506f24{};
    std::uint8_t byte_425009d4{};
    std::uint32_t dword_42505850{};
    std::uint32_t dword_425056e4{};
    std::uint32_t dword_4250853c{};
};

// Explicit local block only; entry, scheduling and snapshot identity are caller
// assumptions. Does not synchronize ChargeState or SmallGateInput projections.
void apply_full_local_charge_reset(ChargeResetSnapshot& snapshot);

// Composes the byte-anchored local transitions; the original caller order for
// the separate small-gate helper is not established by the saved walks.
void compose_charge_model_step(ChargeState& state,
                               const ChargeObservation& observation);

} // namespace rage::offline

#endif

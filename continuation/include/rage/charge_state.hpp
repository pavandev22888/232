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
                                         std::uint8_t mode_result);

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
    std::uint8_t mode_result;
    ChargeCapacityInput capacity;
    SmallGateInput gate;
};

struct ChargeState {
    bool mode_latched{};
    ChargeCapacity capacity{};
    bool small_gate_result{};
};

// Composes the byte-anchored local transitions; the original caller order for
// the separate small-gate helper is not established by the saved walks.
void compose_charge_model_step(ChargeState& state,
                               const ChargeObservation& observation);

} // namespace rage::offline

#endif

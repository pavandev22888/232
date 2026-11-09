#include "rage/charge_state.hpp"

#include <cstring>
#include <limits>

namespace rage::offline {
namespace {
static_assert(sizeof(std::uint8_t) == 1);
static_assert(sizeof(std::int32_t) == 4);
static_assert(sizeof(std::uint32_t) == 4);
static_assert(std::numeric_limits<std::int32_t>::is_signed);

std::int32_t signed_bits(std::uint32_t bits)
{
    std::int32_t value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::int32_t wrapping_add(std::int32_t left, std::int32_t right)
{
    return signed_bits(static_cast<std::uint32_t>(left) +
                       static_cast<std::uint32_t>(right));
}

std::int32_t wrapping_subtract(std::int32_t left, std::int32_t right)
{
    return signed_bits(static_cast<std::uint32_t>(left) -
                       static_cast<std::uint32_t>(right));
}
} // namespace

ChargeCapacity calculate_charge_capacity(const ChargeCapacityInput& input,
                                         std::uint8_t raw_mode_result)
{
    ChargeCapacity out{};
    const bool mode_nonzero = raw_mode_result != 0;

    out.cap = input.raw_limit < 17 ? input.raw_limit : 17;
    if (input.raw_limit == 0)
        out.cap = 59;

    if (mode_nonzero) {
        const auto floor = out.cap >= 2 ? out.cap : 1;
        out.release_limit = wrapping_subtract(floor, 1);
    } else {
        const auto floor = out.cap >= 3 ? out.cap : 2;
        out.release_limit = wrapping_subtract(floor, 2);
    }

    const auto allowed_ceiling = out.cap >= 4
        ? wrapping_subtract(out.cap, 2)
        : 1;
    const auto requested = wrapping_add(
        wrapping_subtract(out.release_limit, input.configured_count), 1);
    out.allowed = requested < allowed_ceiling ? requested : allowed_ceiling;

    if (static_cast<std::uint32_t>(out.release_limit) < 2u)
        return out;

    out.selected = out.allowed;
    if (input.use_allowed_limit)
        return out;

    if ((raw_mode_result & 1u) == 0 && input.option != 2) {
        const auto half = out.cap / 2;
        out.selected = (half > 0 ? half : 0) + 1;
        return out;
    }

    const auto option_ceiling = out.cap >= 3
        ? wrapping_subtract(out.cap, 2)
        : 1;
    const auto signed_cap_minus_two = wrapping_subtract(out.cap, 2);
    out.selected = out.release_limit < signed_cap_minus_two
        ? out.release_limit
        : option_ceiling;
    return out;
}

bool evaluate_small_charge_gate(const SmallGateInput& input,
                                bool mode_latched)
{
    if (!input.guard_a)
        return true;

    if (mode_latched) {
        if (input.current_counter - std::uint32_t{1} ==
            input.previous_counter)
            return false;
        if (input.guard_c)
            return false;
    }

    return input.enabled_d;
}

void apply_charge_reset_slice(ChargeState& state, SmallGateInput& gate)
{
    state.mode_latched = false;
    gate.guard_c = false;
    gate.previous_counter = 0;
}

void apply_full_local_charge_reset(ChargeResetSnapshot& snapshot)
{
    snapshot.byte_425009bc = 0;
    snapshot.byte_4223f42c = 0;
    snapshot.dword_424b1c74 = 0;
    snapshot.dword_42502ba0 = 0;
    snapshot.dword_425057f8 = 0;
    snapshot.dword_4250097c = 0;
    snapshot.dword_42506f24 = 0;
    snapshot.byte_425009d4 = 0;
    snapshot.dword_42505850 = 0;
    snapshot.dword_425056e4 = 0;
    snapshot.dword_4250853c = 0;
}

void compose_charge_model_step(ChargeState& state,
                               const ChargeObservation& observation)
{
    if (observation.raw_mode_result != 0)
        state.mode_latched = true;

    state.capacity = calculate_charge_capacity(observation.capacity,
                                               observation.raw_mode_result);
    state.small_gate_result = evaluate_small_charge_gate(
        observation.gate, state.mode_latched);
}

} // namespace rage::offline

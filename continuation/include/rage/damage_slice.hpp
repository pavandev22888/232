#ifndef RAGE_DAMAGE_SLICE_HPP
#define RAGE_DAMAGE_SLICE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rage::offline {

// Models only the 41370710 hitgroup-3 arm; provider_value is its observed float return.
[[nodiscard]] std::optional<float> scale_hitgroup3_case(
    std::int32_t hitgroup,
    float provider_value) noexcept;

struct DamageHitgroup1Observations {
    std::optional<std::uint32_t> esi_at_41370924;
    // Independent FS:[30] fields, in original load/add order, after the first call.
    std::optional<std::array<std::uint32_t, 5>> peb_a8_a4_b8_bc_64;
    std::optional<std::uint32_t> provider_store_41370a53_bits;
    std::optional<std::int32_t> epoch_42500884_after_second_call;
    std::optional<std::int32_t> tls_epoch_after_second_call;
    std::optional<std::uint8_t> byte_at_41370a86;
};

struct DamageSwitchObservations {
    std::int32_t hitgroup;
    std::uint32_t argument_at_41370a7c_bits;
    // Already rounded by the first call's FSTP; never reused as the second return.
    std::optional<std::uint32_t> provider_store_413708dd_bits;
    DamageHitgroup1Observations hitgroup1;
};

enum class DamageSwitchEdge : std::uint8_t {
    first_provider_unobserved,
    hitgroup1_guard_unobserved,
    hitgroup1_guard_jump_4137097f,
    hitgroup1_guard_jump_41370a23,
    hitgroup1_provider_unobserved,
    hitgroup1_tls_unobserved,
    hitgroup1_initialization_41370b4b,
    hitgroup1_byte_unobserved,
    result_slot_written_41370aa1,
};

struct DamageFloatStore {
    std::uint32_t instruction_va;
    std::uint32_t value_bits;
};

struct DamageSwitchResult {
    DamageSwitchEdge edge;
    std::array<DamageFloatStore, 3> float_stores{};
    std::size_t store_count = 0;
    std::optional<std::uint32_t> result_slot_bits;
};

// Starts at 413708DD; output is the SSE result slot, before FLD/x87 return.
// Arithmetic uses x86 MULSS with the caller's MXCSR, including NaN operand order.
[[nodiscard]] DamageSwitchResult evaluate_damage_switch(
    const DamageSwitchObservations& observations) noexcept;

} // namespace rage::offline

#endif

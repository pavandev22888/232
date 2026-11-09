#include "rage/damage_slice.hpp"

#include <cstring>
#include <limits>

namespace rage::offline {
namespace {
static_assert(sizeof(float) == sizeof(std::uint32_t));
static_assert(std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<float>::digits == 24);

constexpr std::int32_t kHitgroup3 = 3;
constexpr float kHitgroup3Multiplier = 1.25f;

std::uint32_t multiply_binary32(std::uint32_t left, std::uint32_t right) noexcept
{
    float destination;
    float source;
    std::memcpy(&destination, &left, sizeof(left));
    std::memcpy(&source, &right, sizeof(right));
    // MULSS operand order matters for NaN payloads; C++ multiplication may commute.
    __asm__("mulss %1, %0" : "+x"(destination) : "x"(source));
    std::uint32_t result;
    std::memcpy(&result, &destination, sizeof(result));
    return result;
}

void append_store(DamageSwitchResult& result, std::uint32_t va,
                  std::uint32_t bits) noexcept
{
    result.float_stores[result.store_count++] = {va, bits};
}
} // namespace

std::optional<float> scale_hitgroup3_case(
    std::int32_t hitgroup,
    float provider_value) noexcept
{
    if (hitgroup != kHitgroup3)
        return std::nullopt;

    float result = provider_value;
    result *= kHitgroup3Multiplier;
    return result;
}

DamageSwitchResult evaluate_damage_switch(
    const DamageSwitchObservations& observations) noexcept
{
    DamageSwitchResult result{DamageSwitchEdge::first_provider_unobserved, {}, 0, {}};
    if (!observations.provider_store_413708dd_bits)
        return result;

    std::uint32_t value = *observations.provider_store_413708dd_bits;
    append_store(result, 0x413708dd, value);
    const auto index = static_cast<std::uint32_t>(observations.hitgroup) - 1u;
    if (index > 7u) {
        value = 0x3f800000;
    } else if (index == 5u || index == 6u) {
        value = multiply_binary32(value, 0x3f400000);
    } else if (index == 2u) {
        value = multiply_binary32(value, 0x3fa00000);
    } else if (index == 0u) {
        const auto& hitgroup1 = observations.hitgroup1;
        result.edge = DamageSwitchEdge::hitgroup1_guard_unobserved;
        if (!hitgroup1.esi_at_41370924 || !hitgroup1.peb_a8_a4_b8_bc_64)
            return result;
        const auto& fields = *hitgroup1.peb_a8_a4_b8_bc_64;
        std::uint32_t sum = fields[0];
        for (std::size_t i = 1; i < fields.size(); ++i)
            sum += fields[i];
        if (sum != 0x20u) {
            result.edge = *hitgroup1.esi_at_41370924 == 3u
                ? DamageSwitchEdge::hitgroup1_guard_jump_4137097f
                : DamageSwitchEdge::hitgroup1_guard_jump_41370a23;
            return result;
        }
        result.edge = DamageSwitchEdge::hitgroup1_provider_unobserved;
        if (!hitgroup1.provider_store_41370a53_bits)
            return result;
        value = *hitgroup1.provider_store_41370a53_bits;
        append_store(result, 0x41370a53, value);
        result.edge = DamageSwitchEdge::hitgroup1_tls_unobserved;
        if (!hitgroup1.epoch_42500884_after_second_call || !hitgroup1.tls_epoch_after_second_call)
            return result;
        if (*hitgroup1.epoch_42500884_after_second_call > *hitgroup1.tls_epoch_after_second_call) {
            result.edge = DamageSwitchEdge::hitgroup1_initialization_41370b4b;
            return result;
        }
        result.edge = DamageSwitchEdge::hitgroup1_byte_unobserved;
        if (!hitgroup1.byte_at_41370a86)
            return result;
        if (*hitgroup1.byte_at_41370a86 != 0)
            value = multiply_binary32(value, 0x3f000000);
        value = multiply_binary32(value, observations.argument_at_41370a7c_bits);
    }

    append_store(result, 0x41370aa1, value);
    result.result_slot_bits = value;
    result.edge = DamageSwitchEdge::result_slot_written_41370aa1;
    return result;
}

} // namespace rage::offline

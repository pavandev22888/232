#include "rage/damage_slice.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

using namespace rage::offline;

namespace {
int failures = 0;
int checks = 0;

void check(bool condition, const char* name)
{
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

std::uint32_t bits(float value)
{
    std::uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(raw));
    return raw;
}
DamageSwitchObservations hitgroup1_input()
{
    return {1, bits(4.0f), bits(64.0f),
            {3u, std::array<std::uint32_t, 5>{1, 2, 3, 4, 22},
             bits(6.0f), -1, 0, 0}};
}

void check_result(const DamageSwitchResult& result, std::uint32_t expected,
                  std::size_t stores)
{
    check(result.edge == DamageSwitchEdge::result_slot_written_41370aa1,
          "completed route reaches the SSE result slot");
    check(result.result_slot_bits && *result.result_slot_bits == expected,
          "exact result slot bits");
    check(result.store_count == stores, "float store count");
    check(result.store_count != 0 &&
          result.float_stores[result.store_count - 1].instruction_va == 0x41370aa1 &&
          result.float_stores[result.store_count - 1].value_bits == expected,
          "result slot is the final float store");
}

void check_frontier(const DamageSwitchResult& result, DamageSwitchEdge edge,
                    std::size_t stores)
{
    check(result.edge == edge, "exact unresolved or out-of-slice edge");
    check(!result.result_slot_bits, "frontier never manufactures a result");
    check(result.store_count == stores, "frontier retains only preceding stores");
}

void test_switch()
{
    constexpr std::int32_t unchanged[] = {2, 4, 5, 8};
    constexpr std::uint32_t words[] = {
        0u, 0x80000000u, 0x7fc12345u, 0x7f812345u, 0x7f800000u, 0xff800000u};
    for (const auto hitgroup : unchanged) {
        for (const auto word : words) {
            const auto result = evaluate_damage_switch({hitgroup, 0x7fc54321, word, {}});
            check_result(result, word, 2);
            check(result.float_stores[0].instruction_va == 0x413708dd &&
                  result.float_stores[0].value_bits == word,
                  "provider FSTP observation precedes unchanged result");
        }
    }
    for (const auto hitgroup : {3, 6, 7}) {
        const auto coefficient_result = hitgroup == 3 ? bits(5.0f) : bits(3.0f);
        check_result(evaluate_damage_switch({hitgroup, 0, bits(4.0f), {}}),
                     coefficient_result, 2);
        check_result(evaluate_damage_switch({hitgroup, 0, 0x80000000, {}}),
                     0x80000000, 2);
        check_result(evaluate_damage_switch({hitgroup, 0, 0x7f812345, {}}),
                     0x7fc12345, 2);
        check_result(evaluate_damage_switch({hitgroup, 0, 0x7f800000, {}}),
                     0x7f800000, 2);
    }
    constexpr std::int32_t defaults[] = {
        0, -1, 9, std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max()};
    for (const auto hitgroup : defaults) {
        check_result(evaluate_damage_switch({hitgroup, 0, 0x7fc12345, {}}),
                     bits(1.0f), 2);
        check_frontier(evaluate_damage_switch({hitgroup, 0, {}, {}}),
                       DamageSwitchEdge::first_provider_unobserved, 0);
    }
}

void test_hitgroup1_guards()
{
    auto input = hitgroup1_input();
    check_result(evaluate_damage_switch(input), bits(24.0f), 3);
    const auto result = evaluate_damage_switch(input);
    check(result.float_stores[0].instruction_va == 0x413708dd &&
          result.float_stores[0].value_bits == bits(64.0f) &&
          result.float_stores[1].instruction_va == 0x41370a53 &&
          result.float_stores[1].value_bits == bits(6.0f),
          "first and second provider observations remain distinct and ordered");
    input.provider_store_413708dd_bits = 0x7fc54321;
    check_result(evaluate_damage_switch(input), bits(24.0f), 3);
    input.provider_store_413708dd_bits.reset();
    check_frontier(evaluate_damage_switch(input),
                   DamageSwitchEdge::first_provider_unobserved, 0);

    input = hitgroup1_input();
    input.hitgroup1.esi_at_41370924.reset();
    check_frontier(evaluate_damage_switch(input), DamageSwitchEdge::hitgroup1_guard_unobserved, 1);
    input = hitgroup1_input();
    input.hitgroup1.peb_a8_a4_b8_bc_64.reset();
    check_frontier(evaluate_damage_switch(input), DamageSwitchEdge::hitgroup1_guard_unobserved, 1);
    for (const auto esi : {3u, 0u, 0xffffffffu}) {
        input = hitgroup1_input();
        input.hitgroup1.esi_at_41370924 = esi;
        input.hitgroup1.peb_a8_a4_b8_bc_64 = {0xffffffffu, 1, 0, 0, 32};
        check_result(evaluate_damage_switch(input), bits(24.0f), 3);
        input.hitgroup1.peb_a8_a4_b8_bc_64 = {0, 0, 0, 0, 31};
        check_frontier(evaluate_damage_switch(input), esi == 3
                           ? DamageSwitchEdge::hitgroup1_guard_jump_4137097f
                           : DamageSwitchEdge::hitgroup1_guard_jump_41370a23, 1);
    }
    input = hitgroup1_input();
    input.hitgroup1.provider_store_41370a53_bits.reset();
    check_frontier(evaluate_damage_switch(input), DamageSwitchEdge::hitgroup1_provider_unobserved, 1);
    input = hitgroup1_input();
    input.hitgroup1.epoch_42500884_after_second_call.reset();
    check_frontier(evaluate_damage_switch(input), DamageSwitchEdge::hitgroup1_tls_unobserved, 2);
    input = hitgroup1_input();
    input.hitgroup1.tls_epoch_after_second_call.reset();
    check_frontier(evaluate_damage_switch(input), DamageSwitchEdge::hitgroup1_tls_unobserved, 2);
    input = hitgroup1_input();
    input.hitgroup1.epoch_42500884_after_second_call = 0;
    input.hitgroup1.tls_epoch_after_second_call = -1;
    check_frontier(evaluate_damage_switch(input),
                   DamageSwitchEdge::hitgroup1_initialization_41370b4b, 2);
    input.hitgroup1.epoch_42500884_after_second_call = std::numeric_limits<std::int32_t>::max();
    input.hitgroup1.tls_epoch_after_second_call = std::numeric_limits<std::int32_t>::min();
    check_frontier(evaluate_damage_switch(input),
                   DamageSwitchEdge::hitgroup1_initialization_41370b4b, 2);
    input.hitgroup1.epoch_42500884_after_second_call = std::numeric_limits<std::int32_t>::min();
    input.hitgroup1.tls_epoch_after_second_call = std::numeric_limits<std::int32_t>::max();
    check_result(evaluate_damage_switch(input), bits(24.0f), 3);
    input.hitgroup1.epoch_42500884_after_second_call = -1;
    input.hitgroup1.tls_epoch_after_second_call = -1;
    check_result(evaluate_damage_switch(input), bits(24.0f), 3);
    input.hitgroup1.byte_at_41370a86.reset();
    check_frontier(evaluate_damage_switch(input), DamageSwitchEdge::hitgroup1_byte_unobserved, 2);
}

void test_hitgroup1_arithmetic()
{
    for (const auto flag : {0u, 1u, 2u, 0x80u, 0xffu}) {
        auto input = hitgroup1_input();
        input.hitgroup1.byte_at_41370a86 = static_cast<std::uint8_t>(flag);
        check_result(evaluate_damage_switch(input), bits(flag == 0 ? 24.0f : 12.0f), 3);
    }
    auto input = hitgroup1_input();
    input.hitgroup1.provider_store_41370a53_bits = 0x80000000;
    check_result(evaluate_damage_switch(input), 0x80000000, 3);
    input.argument_at_41370a7c_bits = bits(-4.0f);
    check_result(evaluate_damage_switch(input), 0, 3);
    input.hitgroup1.provider_store_41370a53_bits = 0;
    check_result(evaluate_damage_switch(input), 0x80000000, 3);
    input.hitgroup1.provider_store_41370a53_bits = 0x7fc12345;
    input.argument_at_41370a7c_bits = 0x7fc54321;
    check_result(evaluate_damage_switch(input), 0x7fc12345, 3);
    input.hitgroup1.provider_store_41370a53_bits = 0x7f812345;
    input.argument_at_41370a7c_bits = bits(2.0f);
    check_result(evaluate_damage_switch(input), 0x7fc12345, 3);
    input.hitgroup1.provider_store_41370a53_bits = bits(2.0f);
    input.argument_at_41370a7c_bits = 0x7f854321;
    check_result(evaluate_damage_switch(input), 0x7fc54321, 3);
    input.hitgroup1.provider_store_41370a53_bits = 1;
    input.hitgroup1.byte_at_41370a86 = 1;
    input.argument_at_41370a7c_bits = bits(2.0f);
    check_result(evaluate_damage_switch(input), 0, 3);
    input.hitgroup1.provider_store_41370a53_bits = 3;
    check_result(evaluate_damage_switch(input), 4, 3);
    input.hitgroup1.byte_at_41370a86 = 0;
    check_result(evaluate_damage_switch(input), 6, 3);
    input.hitgroup1.provider_store_41370a53_bits = 0x7f800000;
    input.argument_at_41370a7c_bits = 0;
    const auto invalid = evaluate_damage_switch(input);
    check(invalid.result_slot_bits && (*invalid.result_slot_bits & 0x7fffffffu) > 0x7f800000u,
          "infinity times zero produces NaN, not a synthetic finite clamp");
}
} // namespace

int main()
{
    const auto one = scale_hitgroup3_case(3, 1.0f);
    check(one.has_value(), "hitgroup 3 has a modeled result");
    check(one && bits(*one) == bits(1.25f), "multiplier is binary32 1.25");

    const auto fractional = scale_hitgroup3_case(3, 0.8f);
    check(fractional && bits(*fractional) == bits(1.0f),
          "provider float is multiplied before the float result is returned");

    const auto negative_zero = scale_hitgroup3_case(3, -0.0f);
    check(negative_zero && bits(*negative_zero) == bits(-0.0f),
          "positive multiplier preserves negative zero");

    const auto nan = scale_hitgroup3_case(
        3, std::numeric_limits<float>::quiet_NaN());
    check(nan && std::isnan(*nan), "NaN remains NaN through the local multiply");

    constexpr std::int32_t unmodeled[] = {0, 1, 2, 4, 8, -1};
    for (const auto other : unmodeled)
        check(!scale_hitgroup3_case(other, 4.0f),
              "unmodeled hitgroups have no invented fallback result");

    test_switch();
    test_hitgroup1_guards();
    test_hitgroup1_arithmetic();

    if (failures != 0)
        return 1;
    std::cout << "PASS: damage switch, hitgroup1 guards and ordered float stores ("
              << checks << " checks)\n";
    return 0;
}

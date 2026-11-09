#include "rage/charge_state.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

using rage::offline::ChargeCapacity;
using rage::offline::ChargeCapacityInput;
using rage::offline::ChargeObservation;
using rage::offline::ChargeState;
using rage::offline::SmallGateInput;

namespace {
int failures = 0;
int checks = 0;

std::int32_t from_bits(std::uint32_t value)
{
    std::int32_t out;
    std::memcpy(&out, &value, sizeof(out));
    return out;
}

std::int32_t add32(std::int32_t a, std::int32_t b)
{
    return from_bits(static_cast<std::uint32_t>(a) +
                     static_cast<std::uint32_t>(b));
}

std::int32_t sub32(std::int32_t a, std::int32_t b)
{
    return from_bits(static_cast<std::uint32_t>(a) -
                     static_cast<std::uint32_t>(b));
}

ChargeCapacity reference_capacity(const ChargeCapacityInput& in,
                                  std::uint8_t raw_mode_result)
{
    ChargeCapacity out{};
    const bool mode_nonzero = raw_mode_result != 0;
    out.cap = in.raw_limit < 17 ? in.raw_limit : 17;
    if (in.raw_limit == 0)
        out.cap = 59;

    out.release_limit = mode_nonzero
        ? sub32(out.cap >= 2 ? out.cap : 1, 1)
        : sub32(out.cap >= 3 ? out.cap : 2, 2);
    const auto ceiling = out.cap >= 4 ? sub32(out.cap, 2) : 1;
    const auto request = add32(sub32(out.release_limit,
                                    in.configured_count), 1);
    out.allowed = request < ceiling ? request : ceiling;

    if (static_cast<std::uint32_t>(out.release_limit) >= 2u) {
        out.selected = out.allowed;
        if (!in.use_allowed_limit) {
            if ((raw_mode_result & 1u) == 0 && in.option != 2) {
                const auto half = out.cap / 2;
                out.selected = (half > 0 ? half : 0) + 1;
            } else {
                const auto max_count = out.cap >= 3
                    ? sub32(out.cap, 2)
                    : 1;
                out.selected = out.release_limit < sub32(out.cap, 2)
                    ? out.release_limit
                    : max_count;
            }
        }
    }
    return out;
}

bool same(const ChargeCapacity& a, const ChargeCapacity& b)
{
    return a.cap == b.cap && a.release_limit == b.release_limit &&
           a.allowed == b.allowed && a.selected == b.selected;
}

void check(bool condition, const std::string& name)
{
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

void test_capacity_boundaries()
{
    struct Case {
        ChargeCapacityInput in;
        std::uint8_t raw_mode_result;
        ChargeCapacity expected;
    };
    const std::array<Case, 8> cases{{
        {{0, 0, true, 0}, 0, {59, 57, 57, 57}},
        {{0, 0, false, 0}, 0, {59, 57, 57, 30}},
        {{1, 0, true, 0}, 1, {1, 0, 1, 0}},
        {{2, 0, true, 0}, 1, {2, 1, 1, 0}},
        {{3, 0, true, 0}, 1, {3, 2, 1, 1}},
        {{4, 0, true, 0}, 0, {4, 2, 2, 2}},
        {{17, 0, false, 0}, 0, {17, 15, 15, 9}},
        {{18, 0, false, 2}, 1, {17, 16, 15, 15}},
    }};
    for (const auto& test : cases) {
        check(same(rage::offline::calculate_charge_capacity(
                       test.in, test.raw_mode_result),
                   test.expected), "capacity boundary");
    }

    const auto min = std::numeric_limits<std::int32_t>::min();
    const auto max = std::numeric_limits<std::int32_t>::max();
    for (const auto raw : {min, -1, max}) {
        for (const auto config : {min, -1, 0, max}) {
            for (const auto raw_mode_result : {std::uint8_t{0}, std::uint8_t{1},
                                            std::uint8_t{2}, std::uint8_t{3},
                                            std::uint8_t{254}, std::uint8_t{255}}) {
                for (const bool use_allowed : {false, true}) {
                    const ChargeCapacityInput in{raw, config, use_allowed, 2};
                    check(same(rage::offline::calculate_charge_capacity(in,
                                                                        raw_mode_result),
                               reference_capacity(in, raw_mode_result)),
                          "capacity wrapping edge");
                }
            }
        }
    }
}

void test_capacity_contract_grid()
{
    constexpr std::array<std::int32_t, 12> raw{{
        -1, 0, 1, 2, 3, 4, 7, 16, 17, 18, 59, 64
    }};
    constexpr std::array<std::int32_t, 6> config{{0, 1, 2, 5, 10, 64}};
    constexpr std::array<std::int32_t, 3> option{{0, 2, 3}};
    for (const auto r : raw) {
        for (const auto raw_mode_result : {std::uint8_t{0}, std::uint8_t{1},
                                        std::uint8_t{2}, std::uint8_t{3},
                                        std::uint8_t{254}, std::uint8_t{255}}) {
            for (const auto c : config) {
                for (const bool use_allowed : {false, true}) {
                    for (const auto opt : option) {
                        const ChargeCapacityInput in{r, c, use_allowed, opt};
                        check(same(rage::offline::calculate_charge_capacity(
                                      in, raw_mode_result),
                                   reference_capacity(in, raw_mode_result)),
                              "capacity contract grid");
                    }
                }
            }
        }
    }
}

void test_small_gate_conditions()
{
    constexpr std::array<std::uint32_t, 5> now_values{{
        0, 1, 2, 10, 0xffffffffu
    }};
    constexpr std::array<std::uint32_t, 5> previous_values{{
        0, 1, 9, 0xfffffffeu, 0xffffffffu
    }};
    for (const bool a : {false, true}) {
        for (const bool b : {false, true}) {
            for (const bool c : {false, true}) {
                for (const bool d : {false, true}) {
                    for (const auto now : now_values) {
                        for (const auto previous : previous_values) {
                            const SmallGateInput in{a, c, d, now, previous};
                            const bool expected = !a ||
                                (d && !(b && ((now - 1u == previous) || c)));
                            check(rage::offline::evaluate_small_charge_gate(
                                      in, b) == expected,
                                  "small gate condition grid");
                        }
                    }
                }
            }
        }
    }
}

void test_composed_transition_and_latch()
{
    ChargeState state{};
    const ChargeObservation set_mode{
        2, {17, 0, false, 0}, {true, false, true, 0, 0xffffffffu}
    };
    rage::offline::compose_charge_model_step(state, set_mode);
    check(state.mode_latched, "mode result sets local latch");
    check(same(state.capacity, {17, 16, 15, 9}), "raw even mode capacity branch");
    check(!state.small_gate_result, "wrapped counter equality blocks gate");

    const ChargeObservation no_new_mode{
        0, {17, 0, true, 0}, {true, false, true, 2, 0}
    };
    rage::offline::compose_charge_model_step(state, no_new_mode);
    check(state.mode_latched, "false observation preserves local latch");
    check(same(state.capacity, {17, 15, 15, 15}), "non-mode capacity branch");
    check(state.small_gate_result, "latched gate permits nonblocked count");

    state = {};
    const ChargeObservation bypass{
        0, {3, 0, true, 0}, {false, true, false, 1, 0}
    };
    rage::offline::compose_charge_model_step(state, bypass);
    check(!state.mode_latched, "false observation leaves clear latch clear");
    check(state.small_gate_result, "zero first guard returns true");
}

void test_raw_mode_byte_branches()
{
    constexpr std::array<std::uint8_t, 6> raw_results{{0, 1, 2, 3, 254, 255}};
    for (const auto raw_result : raw_results) {
        const ChargeCapacityInput input{17, 0, false, 0};
        const auto actual = rage::offline::calculate_charge_capacity(
            input, raw_result);
        const auto expected_release = raw_result == 0 ? 15 : 16;
        const auto expected_selected = (raw_result & 1u) == 0 ? 9 : 15;
        check(actual.release_limit == expected_release,
              "raw-byte nonzero release branch");
        check(actual.selected == expected_selected,
              "raw-byte low-bit half-selection branch");

        ChargeState state{};
        const ChargeObservation observation{
            raw_result, input, {true, false, true, 2, 0}
        };
        rage::offline::compose_charge_model_step(state, observation);
        check(state.mode_latched == (raw_result != 0),
              "raw-byte nonzero latch branch");
    }
}
} // namespace

int main()
{
    test_capacity_boundaries();
    test_capacity_contract_grid();
    test_small_gate_conditions();
    test_composed_transition_and_latch();
    test_raw_mode_byte_branches();
    if (failures != 0) {
        std::cerr << failures << " of " << checks << " checks failed\n";
        return 1;
    }
    std::cout << "PASS: " << checks << " charge-state checks\n";
    return 0;
}

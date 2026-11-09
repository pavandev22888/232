#include "rage/movement_slice.hpp"

#include <cstdint>
#include <iostream>
#include <optional>

using rage::offline::Float32Bits;
using rage::offline::MovementCallerDecision;
using rage::offline::MovementCallerEdge;
using rage::offline::MovementCallerObservations;
using rage::offline::evaluate_movement_caller_gate;

namespace {
int failures = 0;

void check(bool condition, const char* name)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

MovementCallerObservations make_observations(
    std::optional<std::uint16_t> selector,
    std::optional<std::uint8_t> guard,
    std::optional<std::uint32_t> argument_bits,
    std::optional<std::uint8_t> predicate_al = std::nullopt)
{
    const std::optional<Float32Bits> argument = argument_bits.has_value()
        ? std::optional<Float32Bits>{Float32Bits{*argument_bits}}
        : std::nullopt;
    return {selector, guard, argument, predicate_al};
}

void check_no_argument(const MovementCallerDecision& decision,
                       MovementCallerEdge expected,
                       const char* name)
{
    check(decision.edge == expected, name);
    check(!decision.forwarded_argument.has_value(),
          "edge has no observed forwarded argument");
}

void check_forwarded_argument(const MovementCallerDecision& decision,
                              MovementCallerEdge expected,
                              std::uint32_t expected_bits,
                              const char* name)
{
    check(decision.edge == expected, name);
    check(decision.forwarded_argument.has_value(),
          "observed call argument is recorded");
    if (decision.forwarded_argument.has_value()) {
        check(decision.forwarded_argument->value == expected_bits,
              "predicate argument bits pass through unchanged");
    }
}
} // namespace

int main()
{
    constexpr std::uint32_t qnan_payload = 0x7fc12345u;
    constexpr std::uint32_t negative_zero = 0x80000000u;

    check_no_argument(
        evaluate_movement_caller_gate(make_observations(
            std::nullopt, std::nullopt, std::nullopt)),
        MovementCallerEdge::selector_unobserved,
        "unknown selector remains explicit");
    check_no_argument(
        evaluate_movement_caller_gate(make_observations(
            0x1fu, std::nullopt, std::nullopt)),
        MovementCallerEdge::selector_guard_to_4148d7b9,
        "selector 0x1f takes its early edge before later observations");
    check_no_argument(
        evaluate_movement_caller_gate(make_observations(
            0x40u, 0x81u, qnan_payload, 0x80u)),
        MovementCallerEdge::selector_guard_to_4148d7b9,
        "selector 0x40 takes its early edge before guard and callee result");
    check_no_argument(
        evaluate_movement_caller_gate(make_observations(
            0x41u, std::nullopt, std::nullopt)),
        MovementCallerEdge::guard_unobserved,
        "unknown guard remains explicit after a nonmatching selector");
    check_no_argument(
        evaluate_movement_caller_gate(make_observations(
            0x41u, 0x81u, std::nullopt)),
        MovementCallerEdge::predicate_skipped_to_4148cc8c,
        "guard bit zero skips the callee despite other bits and unknown inputs");

    check_no_argument(
        evaluate_movement_caller_gate(make_observations(
            0x41u, 0x80u, std::nullopt)),
        MovementCallerEdge::predicate_result_unknown,
        "other guard bits do not skip and missing AL stays unknown");
    check_no_argument(
        evaluate_movement_caller_gate(make_observations(
            0x101fu, 0u, std::nullopt)),
        MovementCallerEdge::predicate_result_unknown,
        "selector comparison preserves its 16-bit value");
    check_forwarded_argument(
        evaluate_movement_caller_gate(make_observations(
            0x41u, 0u, qnan_payload)),
        MovementCallerEdge::predicate_result_unknown, qnan_payload,
        "missing provider AL remains unresolved");
    check_no_argument(
        evaluate_movement_caller_gate(make_observations(
            0x41u, 0u, std::nullopt, 0u)),
        MovementCallerEdge::predicate_zero_to_4148d7b7,
        "observed zero AL identifies the edge even with unknown argument bits");
    check_forwarded_argument(
        evaluate_movement_caller_gate(make_observations(
            0x41u, 0u, negative_zero, 0u)),
        MovementCallerEdge::predicate_zero_to_4148d7b7, negative_zero,
        "zero AL takes its edge and preserves negative-zero argument bits");
    check_forwarded_argument(
        evaluate_movement_caller_gate(make_observations(
            0x41u, 0u, qnan_payload, 0x80u)),
        MovementCallerEdge::predicate_nonzero_to_4148cc8c, qnan_payload,
        "any nonzero AL falls through and preserves NaN payload bits");
    check_forwarded_argument(
        evaluate_movement_caller_gate(make_observations(
            0x41u, 0u, 0x00000000u, 1u)),
        MovementCallerEdge::predicate_nonzero_to_4148cc8c, 0x00000000u,
        "positive-zero argument bits pass unchanged");

    if (failures != 0)
        return 1;
    std::cout << "PASS: movement caller edges, unknowns and raw float-bit forwarding\n";
    return 0;
}

#include "rage/movement_slice.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <xmmintrin.h>

using rage::offline::Float32Bits;
using rage::offline::MovementCallerDecision;
using rage::offline::MovementCallerEdge;
using rage::offline::MovementCallerObservations;
using rage::offline::evaluate_movement_caller_gate;
using rage::offline::MovementFs30Fields;
using rage::offline::MovementPredicateObservations;
using rage::offline::MovementPredicateResult;
using rage::offline::MovementPredicateStop;
using rage::offline::evaluate_movement_predicate;
using rage::offline::evaluate_movement_caller_and_predicate;

namespace {
int failures = 0;
int assertions = 0;

void check(bool condition, const char* name)
{
    ++assertions;
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

constexpr MovementFs30Fields passing_guard{0xffffffffu, 1u, 0x10u, 0x10u, 0u};
constexpr MovementFs30Fields other_passing_guard{1u, 2u, 4u, 8u, 17u};

MovementPredicateObservations float_observations(std::uint32_t stored,
                                                std::uint32_t argument)
{
    return {passing_guard, 0u, other_passing_guard, Float32Bits{stored},
            Float32Bits{argument}, true};
}

void check_stop(const MovementPredicateResult& result,
                MovementPredicateStop stop, const char* name)
{
    check(result.stop == stop, name);
    check(!result.return_al.has_value(), "frontier must not manufacture AL");
}

void test_predicate_frontiers()
{
    auto observations = float_observations(0x41100000u, 0x3f100000u);
    observations.first_guard.reset();
    auto result = evaluate_movement_predicate(observations);
    check_stop(result, MovementPredicateStop::first_guard_unobserved,
               "later observations cannot skip missing first FS snapshot");
    check(!result.first_guard_sum && !result.second_guard_sum &&
          !result.bl && !result.floats, "nothing later is consumed");

    observations.first_guard = MovementFs30Fields{0x10000020u, 0u, 0u, 0u, 0u};
    result = evaluate_movement_predicate(observations);
    check_stop(result, MovementPredicateStop::alternate_control_414a518d,
               "first guard compares all 32 bits, not only low byte");
    check(result.first_guard_sum == 0x10000020u && !result.bl,
          "alternate first path records the sum without claiming a return");

    observations.first_guard = passing_guard;
    observations.slot_34_eax.reset();
    result = evaluate_movement_predicate(observations);
    check_stop(result, MovementPredicateStop::slot_34_return_unobserved,
               "missing slot 34 EAX is a provider frontier");
    check(result.first_guard_sum == 0x20u && !result.bl,
          "first sum wraps at 32 bits and BL has not yet been written");

    observations.slot_34_eax = 0u;
    observations.second_guard.reset();
    result = evaluate_movement_predicate(observations);
    check_stop(result, MovementPredicateStop::second_guard_unobserved,
               "first FS snapshot cannot substitute the second read");
    check(result.bl == 1u && !result.second_guard_sum,
          "MOV BL,1 precedes the second guard");

    observations.second_guard = MovementFs30Fields{0x20u, 0u, 0u, 0u, 1u};
    result = evaluate_movement_predicate(observations);
    check_stop(result, MovementPredicateStop::alternate_control_414a523c,
               "second nonmatching sum takes its own opaque path");
    check(result.second_guard_sum == 0x21u && result.bl == 1u && !result.floats,
          "second guard retains BL but cannot invent slot 30 observations");

    observations.second_guard = other_passing_guard;
    observations.slot_30_fstp32.reset();
    result = evaluate_movement_predicate(observations);
    check_stop(result, MovementPredicateStop::slot_30_store_unobserved,
               "missing post-FSTP32 bits are explicit");
    check(result.second_guard_sum == 0x20u && !result.floats,
          "all five second fields participate in the sum");

    observations.slot_30_fstp32 = Float32Bits{0x41100000u};
    observations.argument.reset();
    result = evaluate_movement_predicate(observations);
    check_stop(result, MovementPredicateStop::argument_unobserved,
               "missing argument cannot be replaced with zero");
    check(result.floats && result.floats->squared.value == 0x3f100000u &&
          !result.floats->masked_argument && result.bl == 1u,
          "float operations precede the missing argument load");

    observations.argument = Float32Bits{0x3f100001u};
    observations.cookie_check_returned_normally = false;
    result = evaluate_movement_predicate(observations);
    check_stop(result, MovementPredicateStop::cookie_check_return_unobserved,
               "BL is not yet caller-visible AL before cookie call returns");
    check(result.bl == 0u, "SETBE replaces the initial BL=1");

    observations = {};
    observations.first_guard = passing_guard;
    for (const std::uint32_t eax : {1u, 0x100u, 0x80000000u, 0xffffffffu}) {
        observations.slot_34_eax = eax;
        result = evaluate_movement_predicate(observations);
        check_stop(result, MovementPredicateStop::cookie_check_return_unobserved,
                   "nonzero full EAX bypasses all float inputs");
        check(result.bl == 1u && !result.second_guard_sum && !result.floats,
              "early branch does not read the second FS snapshot or argument");
        observations.cookie_check_returned_normally = true;
        result = evaluate_movement_predicate(observations);
        check(result.stop == MovementPredicateStop::returned_414a52db &&
              result.return_al == 1u, "early normal epilogue exposes AL=1");
        observations.cookie_check_returned_normally = false;
    }
}

void test_predicate_float_operations()
{
    struct Case {
        const char* name;
        std::uint32_t stored, root, scaled, squared, argument, masked;
        std::uint8_t al;
    };
    const Case cases[] = {
        {"exact equality", 0x41100000u, 0x40400000u, 0x3f400000u,
         0x3f100000u, 0x3f100000u, 0x3f100000u, 1u},
        {"one ULP above", 0x41100000u, 0x40400000u, 0x3f400000u,
         0x3f100000u, 0x3f100001u, 0x3f100001u, 0u},
        {"negative magnitude below", 0x41100000u, 0x40400000u, 0x3f400000u,
         0x3f100000u, 0xbf0fffffu, 0x3f0fffffu, 1u},
        {"sqrt rounding forbids algebraic replacement by stored/16",
         0x40000000u, 0x3fb504f3u, 0x3eb504f3u,
         0x3dffffffu, 0x3e000000u, 0x3e000000u, 0u},
        {"rounded squared equality", 0x40000000u, 0x3fb504f3u, 0x3eb504f3u,
         0x3dffffffu, 0x3dffffffu, 0x3dffffffu, 1u},
        {"positive zero", 0u, 0u, 0u, 0u, 0u, 0u, 1u},
        {"negative zeros", 0x80000000u, 0x80000000u, 0x80000000u,
         0u, 0x80000000u, 0u, 1u},
        {"subnormal with DAZ off and rounded square underflow",
         1u, 0x1a3504f3u, 0x193504f3u, 0u, 1u, 1u, 0u},
        {"infinity equality", 0x7f800000u, 0x7f800000u, 0x7f800000u,
         0x7f800000u, 0xff800000u, 0x7f800000u, 1u},
        {"finite threshold and infinite argument", 0x41100000u,
         0x40400000u, 0x3f400000u, 0x3f100000u,
         0x7f800000u, 0x7f800000u, 0u},
        {"negative input makes unordered comparison true", 0xbf800000u,
         0xffc00000u, 0xffc00000u, 0xffc00000u, 0u, 0u, 1u},
        {"negative infinity", 0xff800000u, 0xffc00000u, 0xffc00000u,
         0xffc00000u, 0x3f800000u, 0x3f800000u, 1u},
        {"provider quiet NaN payload", 0x7fc12345u, 0x7fc12345u,
         0x7fc12345u, 0x7fc12345u, 0u, 0u, 1u},
        {"provider signaling NaN quiets at sqrt", 0x7f812345u, 0x7fc12345u,
         0x7fc12345u, 0x7fc12345u, 0u, 0u, 1u},
        {"argument quiet NaN sign is masked", 0x41100000u,
         0x40400000u, 0x3f400000u, 0x3f100000u,
         0xffc23456u, 0x7fc23456u, 1u},
        {"ANDPS does not quiet signaling argument NaN", 0x41100000u,
         0x40400000u, 0x3f400000u, 0x3f100000u,
         0xffa23456u, 0x7fa23456u, 1u},
    };
    for (const auto& item : cases) {
        const auto result = evaluate_movement_predicate(
            float_observations(item.stored, item.argument));
        check(result.stop == MovementPredicateStop::returned_414a52db &&
              result.return_al == item.al && result.bl == item.al, item.name);
        check(result.floats.has_value(), "float path records operations");
        if (result.floats) {
            const auto& trace = *result.floats;
            check(trace.stored.value == item.stored, "raw FSTP32 bits retained");
            check(trace.square_root.value == item.root, "SQRTSS bits");
            check(trace.scaled.value == item.scaled, "first MULSS bits");
            check(trace.squared.value == item.squared, "second MULSS bits");
            check(trace.masked_argument && trace.masked_argument->value == item.masked,
                  "raw ANDPS low-lane bits");
        }
    }
}

void test_caller_composition()
{
    auto observations = float_observations(0x41100000u, 0xbf100001u);
    for (const auto selector : {0x1fu, 0x40u}) {
        const auto result = evaluate_movement_caller_and_predicate(
            static_cast<std::uint16_t>(selector), 0u, observations);
        check(result.caller.edge == MovementCallerEdge::selector_guard_to_4148d7b9 &&
              !result.predicate, "selector guards do not enter the callee");
    }
    auto result = evaluate_movement_caller_and_predicate(
        std::nullopt, 0u, observations);
    check(result.caller.edge == MovementCallerEdge::selector_unobserved &&
          !result.predicate, "unknown selector prevents callee evaluation");
    result = evaluate_movement_caller_and_predicate(0x41u, std::nullopt, observations);
    check(result.caller.edge == MovementCallerEdge::guard_unobserved &&
          !result.predicate, "unknown caller guard prevents callee evaluation");
    result = evaluate_movement_caller_and_predicate(0x41u, 0x81u, observations);
    check(result.caller.edge == MovementCallerEdge::predicate_skipped_to_4148cc8c &&
          !result.predicate, "caller guard bit 0 set skips the callee");

    result = evaluate_movement_caller_and_predicate(0x101fu, 0x80u, observations);
    check(result.predicate && result.predicate->return_al == 0u,
          "new body reaches W7 with zero AL");
    check_forwarded_argument(result.caller,
        MovementCallerEdge::predicate_zero_to_4148d7b7, 0xbf100001u,
        "composition preserves raw argument and W7 zero edge");

    observations.argument = Float32Bits{0xffc12345u};
    result = evaluate_movement_caller_and_predicate(0x41u, 0u, observations);
    check(result.predicate && result.predicate->return_al == 1u,
          "unordered body result reaches W7 with AL=1");
    check_forwarded_argument(result.caller,
        MovementCallerEdge::predicate_nonzero_to_4148cc8c, 0xffc12345u,
        "caller argument stays raw although callee masks its sign");

    observations.cookie_check_returned_normally = false;
    result = evaluate_movement_caller_and_predicate(0x41u, 0u, observations);
    check(result.predicate && result.predicate->bl == 1u &&
          !result.predicate->return_al,
          "pending normal epilogue is not a fabricated caller result");
    check_forwarded_argument(result.caller,
        MovementCallerEdge::predicate_result_unknown, 0xffc12345u,
        "unproven epilogue retains the W7 unknown edge");

    observations.first_guard.reset();
    result = evaluate_movement_caller_and_predicate(0x41u, 0u, observations);
    check(result.predicate && result.predicate->stop ==
          MovementPredicateStop::first_guard_unobserved &&
          result.caller.edge == MovementCallerEdge::predicate_result_unknown,
          "missing body input cannot be bypassed by caller composition");
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

    const unsigned int previous_mxcsr = _mm_getcsr();
    _mm_setcsr((previous_mxcsr & ~0xe07fu) | 0x1f80u);
    test_predicate_frontiers();
    test_predicate_float_operations();
    test_caller_composition();
    _mm_setcsr(previous_mxcsr);

    if (failures != 0)
        return 1;
    std::cout << "PASS: movement caller/callee, guarded frontiers and exact SSE bits ("
              << assertions << " assertions)\n";
    return 0;
}

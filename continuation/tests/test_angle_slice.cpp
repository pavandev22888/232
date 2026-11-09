#include "rage/angle_slice.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <xmmintrin.h>

using namespace rage::offline;

namespace {
int assertions = 0;
int failures = 0;

void check(bool condition, const char* message)
{
    ++assertions;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

using Memory = std::array<std::uint8_t, 0x20>;

void put(Memory& memory, std::size_t offset, std::uint32_t word)
{
    std::memcpy(memory.data() + offset, &word, sizeof(word));
}

std::uint32_t get(const Memory& memory, std::size_t offset)
{
    std::uint32_t word;
    std::memcpy(&word, memory.data() + offset, sizeof(word));
    return word;
}

Memory memory_with(std::uint32_t first = 0, std::uint32_t second = 0)
{
    Memory memory;
    memory.fill(0xa5);
    put(memory, 0x0c, first);
    put(memory, 0x10, second);
    put(memory, 0x14, 0xdeadbeefu);
    return memory;
}

AngleTailResult run(const AngleTailObservations& observations, Memory& memory)
{
    return evaluate_angle_tail(observations, {memory.data(), memory.size()});
}

void test_frontiers_and_guards()
{
    Memory memory = memory_with(0x7f800000u, 0xff800000u);
    const Memory original = memory;
    AngleTailObservations observations;
    auto result = run(observations, memory);
    check(result.stop == AngleTailStop::first_ax_unobserved &&
          !result.selected_input && !result.wrap, "first AX is required");

    observations.call_414af106_ax = 0;
    result = run(observations, memory);
    check(result.stop == AngleTailStop::second_ax_unobserved,
          "first non-two AX needs a distinct second observation");
    observations.call_414af11f_ax = 1;
    result = run(observations, memory);
    check(result.stop == AngleTailStop::saved_word_unobserved,
          "saved-word load precedes second comparison even when it selects zero");
    observations.saved_word_after_second_call = 0xff812345u;
    result = run(observations, memory);
    check(result.stop == AngleTailStop::remainder_store_unobserved &&
          result.selected_input == 0u && result.argument_binary64 == 0u,
          "second AX=1 selects positive zero before binary64 conversion");

    observations = {};
    observations.call_414af106_ax = 2;
    result = run(observations, memory);
    check(result.stop == AngleTailStop::remainder_store_unobserved &&
          result.selected_input == 0u && result.argument_binary64 == 0u,
          "first AX=2 skips second call and saved-word observation");
    check(memory == original && result.stores_completed == 0 && !result.clamp,
          "unresolved calls do not synthesize a store or mathematical remainder");

    struct Conversion { std::uint32_t input; std::uint64_t expected; };
    constexpr Conversion conversions[]{
        {0u, 0u}, {0x80000000u, 0x8000000000000000ull},
        {0x3fc00000u, 0x3ff8000000000000ull},
        {0x7f800000u, 0x7ff0000000000000ull},
        {0xff800000u, 0xfff0000000000000ull},
        {0x7f812345u, 0x7ff82468a0000000ull},
        {0xffc12345u, 0xfff82468a0000000ull},
    };
    for (const std::uint16_t first : {0u, 1u, 0xffffu}) {
        for (const std::uint16_t second : {0u, 2u, 0xffffu}) {
            for (const auto& conversion : conversions) {
                observations = {first, second, conversion.input, std::nullopt};
                result = run(observations, memory);
                check(result.selected_input == conversion.input &&
                      result.argument_binary64 == conversion.expected,
                      "nonmatching AX values forward exact saved bits to CVTSS2SD");
            }
        }
    }
    check(memory == original, "all conversion-only frontiers leave view untouched");

    observations = {2u, std::nullopt, std::nullopt, 0x43b40000u};
    result = evaluate_angle_tail(observations, {nullptr, 0x20});
    check(result.stop == AngleTailStop::invalid_destination_view &&
          result.wrap && result.wrap->wrapped == 0u && !result.clamp &&
          result.stores_completed == 0, "null view is an offline rejection after arithmetic");
    result = evaluate_angle_tail(observations, {memory.data(), 0x17});
    check(result.stop == AngleTailStop::invalid_destination_view && memory == original,
          "short view is rejected without a partial store");
}

void test_wrap_and_scalar_clamp()
{
    struct Case {
        std::uint32_t input;
        bool high;
        bool low;
        std::uint32_t after_upper;
        std::uint32_t wrapped;
        std::uint32_t clamped;
    };
    constexpr Case cases[]{
        {0u, false, false, 0u, 0u, 0u},
        {0x80000000u, false, false, 0x80000000u, 0x80000000u, 0x80000000u},
        {1u, false, false, 1u, 1u, 1u},
        {0x80000001u, false, false, 0x80000001u, 0x80000001u, 0x80000001u},
        {0x42340000u, false, false, 0x42340000u, 0x42340000u, 0x42340000u},
        {0xc2340000u, false, false, 0xc2340000u, 0xc2340000u, 0xc2340000u},
        {0x43340000u, false, false, 0x43340000u, 0x43340000u, 0x42340000u},
        {0xc3340000u, false, false, 0xc3340000u, 0xc3340000u, 0xc2340000u},
        {0x43340001u, true, false, 0xc333ffffu, 0xc333ffffu, 0xc2340000u},
        {0xc3340001u, false, true, 0xc3340001u, 0x4333ffffu, 0x42340000u},
        {0x43350000u, true, false, 0xc3330000u, 0xc3330000u, 0xc2340000u},
        {0xc3350000u, false, true, 0xc3350000u, 0x43330000u, 0x42340000u},
        {0x43b40000u, true, false, 0u, 0u, 0u},
        {0xc3b40000u, false, true, 0xc3b40000u, 0u, 0u},
        {0x44340000u, true, false, 0x43b40000u, 0x43b40000u, 0x42340000u},
        {0xc4340000u, false, true, 0xc4340000u, 0xc3b40000u, 0xc2340000u},
        {0x7f800000u, true, false, 0x7f800000u, 0x7f800000u, 0x42340000u},
        {0xff800000u, false, true, 0xff800000u, 0xff800000u, 0xc2340000u},
        {0x7fc12345u, false, false, 0x7fc12345u, 0x7fc12345u, 0xc2340000u},
        {0xffc12345u, false, false, 0xffc12345u, 0xffc12345u, 0xc2340000u},
        {0x7f812345u, false, false, 0x7f812345u, 0x7f812345u, 0xc2340000u},
        {0xff812345u, false, false, 0xff812345u, 0xff812345u, 0xc2340000u},
    };
    for (const auto& item : cases) {
        Memory memory = memory_with();
        const auto result = run({2u, std::nullopt, std::nullopt, item.input}, memory);
        check(result.stop == AngleTailStop::continued_414af6ad &&
              result.stores_completed == 2 && result.wrap && result.clamp,
              "observed tail reaches its continuation with two stores");
        if (!result.wrap || !result.clamp)
            continue;
        check(result.wrap->above_180 == item.high &&
              result.wrap->below_minus_180 == item.low &&
              result.wrap->after_upper_selection == item.after_upper &&
              result.wrap->wrapped == item.wrapped,
              "strict ordered masks select raw words, including NaN and signed zero");
        check(result.clamp->scalar_min == item.clamped &&
              get(memory, 0x14) == item.clamped,
              "MAXSS then MINSS use second-operand constants for NaN");
        for (std::size_t i = 0; i < memory.size(); ++i) {
            if (i < 0x0c || i >= 0x18)
                check(memory[i] == 0xa5, "stores preserve bytes outside 0c..17");
        }
    }
    Memory memory = memory_with();
    const auto zero = run({2u, std::nullopt, std::nullopt, 0u}, memory);
    check(zero.wrap && zero.wrap->subtract_360 == 0xc3b40000u &&
          zero.wrap->add_360 == 0x43b40000u,
          "both additions execute even when neither selection is taken");
}

void test_packed_clamps()
{
    struct Case { std::uint32_t first, second, max_first, max_second, first_out, second_out; };
    constexpr Case cases[]{
        {0x80000000u, 0u, 0x80000000u, 0u, 0x80000000u, 0u},
        {0u, 0x80000000u, 0u, 0x80000000u, 0u, 0x80000000u},
        {0x7f800000u, 0xff800000u, 0x7f800000u, 0xc3340000u, 0x42b20000u, 0xc3340000u},
        {0xff800000u, 0x7f800000u, 0xc2b20000u, 0x7f800000u, 0xc2b20000u, 0x43340000u},
        {0x7fc12345u, 0xff812345u, 0xc2b20000u, 0xc3340000u, 0xc2b20000u, 0xc3340000u},
        {0x7f812345u, 0xffc12345u, 0xc2b20000u, 0xc3340000u, 0xc2b20000u, 0xc3340000u},
        {0xc2b20000u, 0x43340000u, 0xc2b20000u, 0x43340000u, 0xc2b20000u, 0x43340000u},
        {0xc2b20001u, 0x43340001u, 0xc2b20000u, 0x43340001u, 0xc2b20000u, 0x43340000u},
        {1u, 0x80000001u, 1u, 0x80000001u, 1u, 0x80000001u},
    };
    for (const auto& item : cases) {
        Memory memory = memory_with(item.first, item.second);
        const auto result = run({2u, std::nullopt, std::nullopt, 0x80000000u}, memory);
        check(result.clamp.has_value(), "packed clamp trace is present");
        if (!result.clamp)
            continue;
        check(result.clamp->packed_max == std::array<std::uint32_t, 2>{item.max_first, item.max_second},
              "packed MAXPS returns the second operand on unordered lanes");
        check(result.clamp->packed_min == std::array<std::uint32_t, 2>{item.first_out, item.second_out} &&
              get(memory, 0x0c) == item.first_out && get(memory, 0x10) == item.second_out,
              "packed MINPS and eight-byte store preserve independent lane results");
        check(get(memory, 0x14) == 0x80000000u,
              "scalar store consumes observed FSTP32, not the old destination word");
    }
}

} // namespace

int main()
{
    const unsigned saved_mxcsr = _mm_getcsr();
    _mm_setcsr(0x1f80u);
    test_frontiers_and_guards();
    test_wrap_and_scalar_clamp();
    test_packed_clamps();
    _mm_setcsr(saved_mxcsr);
    std::cout << "angle_slice: " << assertions << " assertions, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}

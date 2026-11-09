#include "rage/release_state_slice.hpp"

#include <cstdlib>
#include <iostream>

using namespace rage::offline;
namespace {
unsigned assertions = 0;
#define CHECK(c) do { ++assertions; if (!(c)) { \
    std::cerr << "FAIL " << __LINE__ << ": " #c "\n"; std::exit(1); } } while (false)
}
int main()
{
    for (unsigned raw = 0; raw <= 255; ++raw) {
        const auto b = static_cast<std::uint8_t>(raw);
        CHECK(release_latch_route(0, b) == ReleaseLatchRoute::alternate_415e057c);
        if (raw != 0) {
            CHECK(release_latch_route(b, 0) == ReleaseLatchRoute::dispatch_415e01d2);
            CHECK(release_latch_route(b, b) == ReleaseLatchRoute::helper_414f2240);
        }
        for (const auto old : {0u, 1u, 0x80u, 0xffu}) {
            for (const auto current : {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu}) {
                const ReleaseLatchSnapshot before{static_cast<std::uint8_t>(old), 99, b};
                const auto after = apply_release_after_helper(before, b, current);
                CHECK(after.byte_42506f30 == (raw == 0 ? 1 : old));
                CHECK(after.dword_4223fe28 == current);
                CHECK(after.byte_42500c78 == 0);
                CHECK(before.dword_4223fe28 == 99 && before.byte_42500c78 == b);
            }
        }
    }
    for (const auto counter : {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu}) {
        CHECK(!release_counter_after_dispatch(counter, counter, std::nullopt));
        CHECK(release_counter_after_dispatch(counter, counter, 0) == counter);
        CHECK(release_counter_after_dispatch(counter, counter + 1u, std::nullopt) == counter);
        for (const auto field : {1u, 0x80000000u, 0xffffffffu}) {
            CHECK(release_counter_after_dispatch(counter, counter, field) == counter + 1u);
            CHECK(release_counter_after_dispatch(counter, counter + 1u, field) == counter);
        }
    }
    std::cout << assertions << " release-state assertions passed\n";
}

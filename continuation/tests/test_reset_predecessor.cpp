#include "rage/reset_predecessor.hpp"

#include <cstdint>
#include <iostream>

using rage::offline::ChargeResetSnapshot;
using rage::offline::ResetPredecessorBranch;
using rage::offline::apply_reset_predecessor_projection;

namespace {
int failures = 0;

void check(bool condition, const char* name)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

ChargeResetSnapshot seeded_snapshot()
{
    return {0xa5u, 0x5au, 0x01020304u, 0x11121314u, 0x21222324u,
            0x31323334u, 0x41424344u, 0x96u, 0x51525354u, 0x61626364u,
            0x71727374u};
}

bool same_snapshot(const ChargeResetSnapshot& left,
                   const ChargeResetSnapshot& right)
{
    return left.byte_425009bc == right.byte_425009bc &&
           left.byte_4223f42c == right.byte_4223f42c &&
           left.dword_424b1c74 == right.dword_424b1c74 &&
           left.dword_42502ba0 == right.dword_42502ba0 &&
           left.dword_425057f8 == right.dword_425057f8 &&
           left.dword_4250097c == right.dword_4250097c &&
           left.dword_42506f24 == right.dword_42506f24 &&
           left.byte_425009d4 == right.byte_425009d4 &&
           left.dword_42505850 == right.dword_42505850 &&
           left.dword_425056e4 == right.dword_425056e4 &&
           left.dword_4250853c == right.dword_4250853c;
}

bool is_zeroed(const ChargeResetSnapshot& value)
{
    return value.byte_425009bc == 0 && value.byte_4223f42c == 0 &&
           value.dword_424b1c74 == 0 && value.dword_42502ba0 == 0 &&
           value.dword_425057f8 == 0 && value.dword_4250097c == 0 &&
           value.dword_42506f24 == 0 && value.byte_425009d4 == 0 &&
           value.dword_42505850 == 0 && value.dword_425056e4 == 0 &&
           value.dword_4250853c == 0;
}

void check_repeat(std::uint32_t input, const char* name)
{
    auto snapshot = seeded_snapshot();
    const auto before = snapshot;
    check(apply_reset_predecessor_projection(input, snapshot) ==
              ResetPredecessorBranch::repeat_loop,
          name);
    check(same_snapshot(snapshot, before),
          "loop branch leaves reset projection unchanged");
}
} // namespace

int main()
{
    constexpr std::uint32_t eax_before_add_for_equal = 0x42502fd4u;

    auto equal_snapshot = seeded_snapshot();
    check(apply_reset_predecessor_projection(eax_before_add_for_equal,
                                             equal_snapshot) ==
              ResetPredecessorBranch::enter_reset_block,
          "equality falls through into reset block");
    check(is_zeroed(equal_snapshot), "equality applies all eleven local stores");

    check_repeat(eax_before_add_for_equal - 1u,
                 "one below repeats the loop");
    check_repeat(eax_before_add_for_equal + 1u,
                 "one above repeats the loop");
    check_repeat(0xffffff38u, "32-bit add wraps before equality");

    if (failures != 0)
        return 1;
    std::cout << "PASS: reset predecessor equality, non-equality and uint32 wrap\n";
    return 0;
}

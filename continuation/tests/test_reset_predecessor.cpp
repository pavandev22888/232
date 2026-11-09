#include "rage/reset_predecessor.hpp"

#include <array>
#include <cstdint>
#include <iostream>

using rage::offline::ChargeResetSnapshot;
using rage::offline::ResetPredecessorBranch;
using rage::offline::ResetPredecessorRecord;
using rage::offline::ResetPredecessorSnapshot;
using rage::offline::ResetStoreWidth;
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

ResetPredecessorSnapshot seeded_projection()
{
    return {seeded_snapshot(), {0x11111111u, 0x22222222u,
                                0x33333333u, 0x44u}};
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

bool same_projection(const ResetPredecessorSnapshot& left,
                     const ResetPredecessorSnapshot& right)
{
    return same_snapshot(left.charge_reset, right.charge_reset) &&
           left.fallthrough.dword_424fdda8 ==
               right.fallthrough.dword_424fdda8 &&
           left.fallthrough.dword_424f4b7c ==
               right.fallthrough.dword_424f4b7c &&
           left.fallthrough.dword_425008cc ==
               right.fallthrough.dword_425008cc &&
           left.fallthrough.byte_425009e9 ==
               right.fallthrough.byte_425009e9;
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

    auto projection = seeded_projection();
    ResetPredecessorRecord record{};
    check(apply_reset_predecessor_projection(eax_before_add_for_equal,
                                             projection, record) ==
              ResetPredecessorBranch::enter_reset_block,
          "recorded equality enters local reset block");
    check(record.eax_base == eax_before_add_for_equal,
          "body record retains caller-supplied EAX base");
    check(record.body_stores.size() == 25u,
          "body record contains 25 dword stores");
    check(sizeof(record.body_stores.front().value) == 4u,
          "body store value has DWORD width");
    bool ordered_body = true;
    for (std::size_t index = 0; index < record.body_stores.size(); ++index) {
        const auto& store = record.body_stores[index];
        ordered_body = ordered_body && store.offset == index * 8u &&
                       store.value == 0xffffffffu;
    }
    check(ordered_body, "body dword stores are ordered from offset 0 to 0xc0");
    check(record.body_stores.front().offset == 0u &&
              record.body_stores.back().offset == 0xc0u &&
              record.body_stores.back().offset +
                      sizeof(record.body_stores.back().value) == 0xc4u,
          "body dword record bounds cover offsets 0 through 0xc3");

    constexpr std::array<std::uint32_t, 4> prefix_addresses{{
        0x424fdda8u, 0x424f4b7cu, 0x425008ccu, 0x425009e9u}};
    constexpr std::array<std::uint32_t, 4> prefix_values{{
        0xffffffffu, 0u, 0xffffffffu, 0u}};
    constexpr std::array<ResetStoreWidth, 4> prefix_widths{{
        ResetStoreWidth::dword, ResetStoreWidth::dword,
        ResetStoreWidth::dword, ResetStoreWidth::byte}};
    check(record.fallthrough_store_count == prefix_addresses.size(),
          "equality records four fallthrough prefix stores");
    bool ordered_prefix = true;
    for (std::size_t index = 0; index < prefix_addresses.size(); ++index) {
        const auto& store = record.fallthrough_stores[index];
        ordered_prefix = ordered_prefix &&
                         store.address == prefix_addresses[index] &&
                         store.value == prefix_values[index] &&
                         store.width == prefix_widths[index];
    }
    check(ordered_prefix,
          "prefix records preserve address, value, width and instruction order");
    check(sizeof(projection.fallthrough.dword_424fdda8) == 4u &&
              sizeof(projection.fallthrough.dword_424f4b7c) == 4u &&
              sizeof(projection.fallthrough.dword_425008cc) == 4u &&
              sizeof(projection.fallthrough.byte_425009e9) == 1u,
          "fallthrough snapshot fields retain byte and dword widths");
    check(projection.fallthrough.dword_424fdda8 == 0xffffffffu &&
              projection.fallthrough.dword_424f4b7c == 0u &&
              projection.fallthrough.dword_425008cc == 0xffffffffu &&
              projection.fallthrough.byte_425009e9 == 0u,
          "fallthrough snapshot applies four source-backed values");
    check(is_zeroed(projection.charge_reset),
          "recorded equality applies existing eleven-store snapshot");

    auto repeated_projection = seeded_projection();
    const auto repeated_before = repeated_projection;
    ResetPredecessorRecord repeated_record{};
    check(apply_reset_predecessor_projection(eax_before_add_for_equal - 1u,
                                             repeated_projection,
                                             repeated_record) ==
              ResetPredecessorBranch::repeat_loop,
          "recorded non-equality repeats loop");
    check(same_projection(repeated_projection, repeated_before),
          "non-equality leaves prefix and charge snapshots unchanged");
    check(repeated_record.fallthrough_store_count == 0u,
          "non-equality records no fallthrough prefix stores");
    check(repeated_record.body_stores.size() == 25u &&
              repeated_record.body_stores.front().offset == 0u &&
              repeated_record.body_stores.back().offset == 0xc0u,
          "non-equality still records the preceding body iteration");

    if (failures != 0)
        return 1;
    std::cout << "PASS: reset predecessor branch, byte-backed store order, widths and bounds\n";
    return 0;
}

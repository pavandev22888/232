#include "rage/reset_predecessor.hpp"

#include <cstdint>
#include <iostream>

using namespace rage::offline;

namespace {
unsigned checks = 0;
unsigned failures = 0;

void check(bool condition, const char* message)
{
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void check_globals(const ResetPredecessorSnapshot& globals)
{
    const auto& c = globals.charge_reset;
    check(c.byte_425009bc == 0 && c.byte_4223f42c == 0 &&
              c.dword_424b1c74 == 0 && c.dword_42502ba0 == 0 &&
              c.dword_425057f8 == 0 && c.dword_4250097c == 0 &&
              c.dword_42506f24 == 0 && c.byte_425009d4 == 0 &&
              c.dword_42505850 == 0 && c.dword_425056e4 == 0 &&
              c.dword_4250853c == 0,
          "all eleven accepted charge stores");
    const auto& f = globals.fallthrough;
    check(f.dword_424fdda8 == 0xffffffffu && f.dword_424f4b7c == 0 &&
              f.dword_425008cc == 0xffffffffu && f.byte_425009e9 == 0,
          "all four accepted fallthrough stores");
}
} // namespace

int main()
{
    check(kResetTableRecordCount == 6, "six source-backed records");
    for (unsigned pattern = 0; pattern != 3; ++pattern) {
        ResetTableSnapshot before{};
        for (std::size_t row = 0; row < before.records.size(); ++row) {
            for (std::size_t byte = 0; byte < before.records[row].size(); ++byte) {
                before.records[row][byte] = pattern == 0 ? 0 : pattern == 1 ? 255 :
                    static_cast<std::uint8_t>(row * 31 + byte * 17 + 9);
            }
        }
        before.globals = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
                          {12, 13, 14, 15}};
        const auto input = before;
        const auto after = apply_reset_table_snapshot(before);
        check(before.records == input.records, "input records are immutable");
        check(before.globals.charge_reset.byte_425009bc == 1 &&
                  before.globals.charge_reset.dword_4250853c == 11 &&
                  before.globals.fallthrough.dword_424fdda8 == 12 &&
                  before.globals.fallthrough.byte_425009e9 == 15,
              "input globals are immutable");
        for (std::size_t row = 0; row < after.records.size(); ++row) {
            for (std::size_t byte = 0; byte < after.records[row].size(); ++byte) {
                const auto expected = byte % 8 < 4 ? 255 : input.records[row][byte];
                check(after.records[row][byte] == expected,
                      "150 DWORD stores preserve all interleaved gaps and tails");
            }
        }
        check_globals(after.globals);
        const auto twice = apply_reset_table_snapshot(after);
        check(twice.records == after.records, "table reset is idempotent");
        check_globals(twice.globals);
    }
    std::cout << checks << " reset table checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}

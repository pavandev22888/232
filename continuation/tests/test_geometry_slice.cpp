#include "rage/geometry_slice.hpp"

#include <cstdint>
#include <iostream>
#include <xmmintrin.h>

using rage::offline::consume_geometry_postcall;
using rage::offline::GeometryOutputBytes;
using rage::offline::GeometryPostCallBytes;

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

template<std::size_t N>
void put_word(std::array<std::uint8_t, N>& bytes, std::size_t offset,
              std::uint32_t word)
{
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(word >> (8 * i));
}

std::uint32_t word_at(const GeometryOutputBytes& bytes, std::size_t offset)
{
    return std::uint32_t{bytes[offset]} | (std::uint32_t{bytes[offset + 1]} << 8) |
        (std::uint32_t{bytes[offset + 2]} << 16) |
        (std::uint32_t{bytes[offset + 3]} << 24);
}

GeometryPostCallBytes observed(std::uint32_t scalar, std::uint8_t flag)
{
    GeometryPostCallBytes bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<std::uint8_t>(0x80 + i);
    put_word(bytes, 0, scalar);
    bytes[0x20] = flag;
    return bytes;
}

void accepted(std::uint32_t scalar, std::uint32_t old_output,
              std::uint32_t expected_product, std::uint8_t flag)
{
    const auto input = observed(scalar, flag);
    const auto input_before = input;
    GeometryOutputBytes output;
    output.fill(0xa5);
    put_word(output, 0x2c, old_output);
    auto expected = output;
    put_word(expected, 0x2c, expected_product);
    if (flag != 0)
        expected[0x37] = 1;
    for (std::size_t i = 0; i < 24; ++i)
        expected[0x0c + i] = input[0x04 + i];

    check(consume_geometry_postcall(input, output) == 0,
          "ordered scalar below one returns original success integer");
    check(output == expected, "exact product, sticky byte and 24-byte stores only");
    check(input == input_before, "externally observed snapshot is not modified");
}
} // namespace

int main()
{
    const unsigned saved_mxcsr = _mm_getcsr();
    _mm_setcsr(0x1f80); // Masked exceptions, nearest-even, no FTZ/DAZ.

    for (const std::uint32_t scalar : {
             0x3f800000u, 0x3f800001u, 0x7f800000u,
             0x7fc12345u, 0xffc12345u, 0x7f812345u}) {
        const auto input = observed(scalar, 0xff);
        GeometryOutputBytes output;
        output.fill(0x5a);
        put_word(output, 0x2c, 0x7f812345);
        const auto before = output;
        _mm_setcsr(0x1f80);
        check(consume_geometry_postcall(input, output) == -1,
              "equal, above, positive infinity and unordered reject");
        check(output == before, "rejection skips every output store");
        const bool invalid = (_mm_getcsr() & 1) != 0;
        check(invalid == (scalar == 0x7f812345u),
              "UCOMISS raises invalid for signaling NaN only, without multiplying output");
    }

    accepted(0x3f000000, 0x41400000, 0x40c00000, 0); // 0.5 * 12 = 6
    accepted(0x3f000000, 0x41400000, 0x40c00000, 1);
    accepted(0x3f000000, 0x41400000, 0x40c00000, 0x80);
    accepted(0x3f000000, 0x41400000, 0x40c00000, 0xff);
    accepted(0x3f7fffff, 0x3f800000, 0x3f7fffff, 0);
    accepted(0xc0000000, 0x40800000, 0xc1000000, 0); // -2 * 4 = -8
    accepted(0x80000000, 0x40800000, 0x80000000, 0);
    accepted(0x00000000, 0xc0800000, 0x80000000, 0);
    accepted(0x80000000, 0xc0800000, 0x00000000, 0);
    accepted(0x00000001, 0x3f800000, 0x00000001, 0);
    accepted(0xff800000, 0x3f800000, 0xff800000, 0);
    accepted(0x3f000000, 0xffc12345, 0xffc12345, 0);
    accepted(0x3f000000, 0x7f812345, 0x7fc12345, 0);

    auto input = observed(0x3f000000, 1);
    struct GuardedOutput {
        std::array<std::uint8_t, 7> prefix;
        GeometryOutputBytes bytes;
        std::array<std::uint8_t, 9> suffix;
    } guarded{};
    guarded.prefix.fill(0x11);
    guarded.suffix.fill(0x22);
    put_word(guarded.bytes, 0x2c, 0x41800000); // 16
    check(consume_geometry_postcall(input, guarded.bytes) == 0,
          "first accepted result stored");
    input[0x20] = 0;
    input[0x1c] = 0xff;
    check(consume_geometry_postcall(input, guarded.bytes) == 0,
          "next result uses previous output scalar");
    check(word_at(guarded.bytes, 0x2c) == 0x40800000,
          "two half factors produce four from sixteen");
    check(guarded.bytes[0x37] == 1, "zero observed flag does not clear prior sticky one");
    check(guarded.bytes[0x23] == input[0x1b], "copy stops before unknown observation gap");
    for (const auto byte : guarded.prefix)
        check(byte == 0x11, "bytes before output untouched");
    for (const auto byte : guarded.suffix)
        check(byte == 0x22, "bytes after minimum output untouched");

    _mm_setcsr(saved_mxcsr);
    if (failures != 0)
        return 1;
    std::cout << "PASS geometry post-call consumer: " << checks << " checks\n";
    return 0;
}

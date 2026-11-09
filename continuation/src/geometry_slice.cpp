#include "rage/geometry_slice.hpp"

#include <cstring>
#include <limits>
#include <xmmintrin.h>

namespace rage::offline {
namespace {
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<float>::digits == 24);
static_assert(sizeof(GeometryPostCallBytes) == 0x21);
static_assert(sizeof(GeometryOutputBytes) == 0x38);

float read_scalar(const std::uint8_t* bytes) noexcept
{
    const std::uint32_t bits = std::uint32_t{bytes[0]} |
        (std::uint32_t{bytes[1]} << 8) | (std::uint32_t{bytes[2]} << 16) |
        (std::uint32_t{bytes[3]} << 24);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void write_scalar(std::uint8_t* bytes, float value) noexcept
{
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    for (std::size_t i = 0; i < sizeof(bits); ++i)
        bytes[i] = static_cast<std::uint8_t>(bits >> (8 * i));
}
} // namespace

std::int32_t consume_geometry_postcall(
    const GeometryPostCallBytes& observation,
    GeometryOutputBytes& output) noexcept
{
    const __m128 scalar = _mm_set_ss(read_scalar(observation.data()));
    // UCOMISS 1.0,scalar / JBE rejects equality, greater values and unordered.
    if (!_mm_ucomigt_ss(_mm_set_ss(1.0f), scalar))
        return -1;

    const __m128 product = _mm_mul_ss(
        scalar, _mm_set_ss(read_scalar(output.data() + 0x2c)));
    write_scalar(output.data() + 0x2c, _mm_cvtss_f32(product));
    if (observation[0x20] != 0)
        output[0x37] = 1;
    std::memcpy(output.data() + 0x0c, observation.data() + 0x04, 16);
    std::memcpy(output.data() + 0x1c, observation.data() + 0x14, 8);
    return 0;
}

} // namespace rage::offline

#include "rage/angle_slice.hpp"

#include <cstring>
#include <emmintrin.h>
#include <limits>

namespace rage::offline {
namespace {

static_assert(sizeof(float) == sizeof(std::uint32_t));
static_assert(std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<float>::digits == 24);

__m128 lane(std::uint32_t bits) noexcept
{
    __m128 value = _mm_setzero_ps();
    std::memcpy(&value, &bits, sizeof(bits));
    return value;
}

std::uint32_t bits(__m128 value) noexcept
{
    std::uint32_t word;
    std::memcpy(&word, &value, sizeof(word));
    return word;
}

std::array<std::uint32_t, 2> low_pair(__m128 value) noexcept
{
    std::array<std::uint32_t, 2> words{};
    std::memcpy(words.data(), &value, sizeof(words));
    return words;
}

__m128 select(__m128 mask, __m128 taken, __m128 other) noexcept
{
    return _mm_or_ps(_mm_and_ps(taken, mask), _mm_andnot_ps(mask, other));
}

} // namespace

AngleTailResult evaluate_angle_tail(
    const AngleTailObservations& observations,
    AngleTailDestination destination) noexcept
{
    AngleTailResult result;
    if (!observations.call_414af106_ax)
        return result;

    std::uint32_t selected = 0;
    if (*observations.call_414af106_ax != 2u) {
        result.stop = AngleTailStop::second_ax_unobserved;
        if (!observations.call_414af11f_ax)
            return result;

        result.stop = AngleTailStop::saved_word_unobserved;
        if (!observations.saved_word_after_second_call)
            return result;

        selected = *observations.saved_word_after_second_call;
        if (*observations.call_414af11f_ax == 1u)
            selected = 0;
    }
    result.selected_input = selected;
    const __m128d argument = _mm_cvtss_sd(_mm_setzero_pd(), lane(selected));
    std::uint64_t argument_bits;
    std::memcpy(&argument_bits, &argument, sizeof(argument_bits));
    result.argument_binary64 = argument_bits;

    result.stop = AngleTailStop::remainder_store_unobserved;
    if (!observations.call_414af152_fstp32)
        return result;

    const __m128 remainder = lane(*observations.call_414af152_fstp32);
    const __m128 subtract_360 = _mm_add_ss(_mm_set_ss(-360.0f), remainder);
    const __m128 upper_mask = _mm_cmplt_ss(_mm_set_ss(180.0f), remainder);
    const __m128 after_upper = select(upper_mask, subtract_360, remainder);
    const __m128 add_360 = _mm_add_ss(_mm_set_ss(360.0f), after_upper);
    const __m128 lower_mask = _mm_cmplt_ss(after_upper, _mm_set_ss(-180.0f));
    const __m128 wrapped = select(lower_mask, add_360, after_upper);
    result.wrap = AngleWrapTrace{
        bits(subtract_360), bits(upper_mask) != 0, bits(after_upper),
        bits(add_360), bits(lower_mask) != 0, bits(wrapped)};

    result.stop = AngleTailStop::invalid_destination_view;
    if (destination.data == nullptr || destination.size < 0x18u)
        return result;

    __m128 packed = _mm_setzero_ps();
    std::memcpy(&packed, destination.data + 0x0c, 8);
    const __m128 packed_max = _mm_max_ps(
        packed, _mm_setr_ps(-89.0f, -180.0f, 0.0f, 0.0f));
    const __m128 packed_min = _mm_min_ps(
        packed_max, _mm_setr_ps(89.0f, 180.0f, 0.0f, 0.0f));
    __m128 scalar_max = _mm_max_ss(wrapped, _mm_set_ss(-45.0f));

    std::memcpy(destination.data + 0x0c, &packed_min, 8);
    // Keep MOVLPS -> MINSS -> MOVSS despite disjoint destination offsets.
    __asm__ __volatile__("" : "+x"(scalar_max) : : "memory");
    ++result.stores_completed;
    const __m128 scalar_min = _mm_min_ss(scalar_max, _mm_set_ss(45.0f));
    std::memcpy(destination.data + 0x14, &scalar_min, 4);
    ++result.stores_completed;

    result.clamp = AngleClampTrace{
        low_pair(packed_max), low_pair(packed_min),
        bits(scalar_max), bits(scalar_min)};
    result.stop = AngleTailStop::continued_414af6ad;
    return result;
}

} // namespace rage::offline

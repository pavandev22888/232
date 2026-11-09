// Unity/LTO must see the multiply to expose reuse across MXCSR changes.
#include "../src/damage_slice.cpp"

#include <cstdio>
#include <xmmintrin.h>

int main()
{
    const unsigned saved = _mm_getcsr();
    rage::offline::DamageSwitchObservations observation{3, 0, 0x3f800001u, {}};
    _mm_setcsr(0x1f80);
    const auto nearest = rage::offline::evaluate_damage_switch(observation);
    _mm_setcsr(0x5f80);
    const auto upward = rage::offline::evaluate_damage_switch(observation);
    _mm_setcsr(saved);

    const auto first = nearest.result_slot_bits.value_or(0);
    const auto second = upward.result_slot_bits.value_or(0);
    std::printf("damage MXCSR: nearest=%08x, upward=%08x\n", first, second);
    return first == 0x3fa00001u && second == 0x3fa00002u ? 0 : 1;
}

# W6 history slice: bounded local store prefix

Source is the read-only handoff's `workspace/work/nl_handoff_2026-11-06/nl.bin`, SHA-256 `682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e`. Fresh byte verification also confirms that this whole 0x3501000-byte image equals the resource at file offset 0x629B10 in `neverlose.dll` (SHA-256 `c1ce8c6d6f1172114d3cff6efe147903292be5eaa6e1c2e28495440fa8f7982c`). Raw disassembly uses `.data` adjusted to `0x412A0000`, making VA `0x413F6DE0` file offset `0x156DE0`; exact source bytes, decoded windows and verification output are beside this note.

## Confirmed local slice

In `0x413F6DE0`, the pointer saved at `[ebp-0x28]` is read as the source at `0x413F7102`; the pointer returned by `0x413F88F0` is used as the destination. The local stores from `0x413F7105` through `0x413F7200` copy 21 four-byte words from source offsets `+0x10,+0x14,+0x18,+0x7C,+0x80,+0x84,+0x88,+0x8C,+0x90,+0x38,+0x4C,+0x1C,+0x20,+0x24,+0x94,+0x98,+0x28,+0x2C,+0x30,+0x5C,+0x40` to destination offsets `+0,+4,+8,+0x24,+0x28,+0x2C,+0x30,+0x34,+0x38,+0x80,+0x94,+0x18,+0x1C,+0x20,+0xA0,+0x70,+0x64,+0x68,+0x6C,+0x98,+0x40`, respectively. Nineteen loads/stores are `MOVSS` float32 bit copies; source `+0x38`, source `+0x40`, and local `[ebp-0x38]` are raw 32-bit words. Destination `+0x3C` receives that local word and `+0xB0` is set to zero. No arithmetic is performed in this slice.

`history_slice.cpp` models only those ordered stores after the source pointer, slot pointer, and local word are already resolved. It does not implement the selector, the helper's allocation/growth, the preceding provider call, or the following conditional/matrix path. Byte views preserve the distinct pointer roles without asserting that they identify the same actor/entity; ordered word copies also avoid an aliasing assumption.

## Exact frontier / unresolved

- The entry path at `0x413F6E1A` calls vtable slot `+0x28` through `[arg+8]`, then `0x41343B20`; its returned pointer and `[result+0xB0]` gate have no established target identity or suitability meaning.
- The helper `0x413F88F0` calls `0x413F3890` and `0x41DA7BAC` before the caller uses its EAX as the destination. Capacity/allocator semantics, slot lifetime, and source-to-slot identity are not established.
- `0x41B70930` is called at `0x413F70FD` before the modeled stores; its side effects are unresolved. The branch `test eax,eax` begins at `0x413F7201`; it is outside the implemented slice.
- The following call at `0x413F723D` targets `0x41B707E0`; no matrix/pose contract or link to point selection is claimed.

The C++17 module is an offline field-store model, not evidence of the original source language, reachability, full restoration, candidate suitability, matrix identity, or game behavior. No original binary was loaded or executed.

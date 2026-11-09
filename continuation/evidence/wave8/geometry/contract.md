# W8-GEOMETRY: observed post-call result consumer

Base: `9b2a75fbd2856fd4966cb6bf6c4933077c51bf41`. This adds an executable
**local consumer**, not an external trace provider or a full geometry routine.
The selected contiguous body is `0x414F3E60..0x414F3E9F`, with return through
`0x414F3EA9`. W6 had documented these effects but supplied no implementation.
W8 adds the exact raw snapshot layout, SSE branch/multiply implementation,
edge-case tests, fresh window hashes and a standalone source verifier.

## Source and boundaries

`source_anchors.json` pins the read-only source ZIP, its `nl.bin` entry and the
identical resource at file offset `0x629B10` of pinned `neverlose.dll`.
Addresses use **conditional file offset + 0x412A0000**, not a proved runtime base.
`objdump.txt` is fresh static x86 decoding; no original code was loaded, run,
emulated or injected. The new 63-byte consumer window has SHA-256
`6530efbee3eb144ae52276436621a624393f9d16b79b681c685b13181e94efe1`.

The W7 helper `0x4136FFA0` was inspected without touching damage files:
its `0x413700E6` indirect `[vtable+0x14]` call is followed only by the cookie
check and epilogue (`trace_wrapper_tail` pin). It supplies no local result
transform there. No identity/dataflow link between that trace buffer and
`0x414F3C90` or `0x41370710` is asserted. The implementation instead stops at
the permitted, independently byte-backed `0x414F3C90` post-call consumer.

The caller passes `ECX = EBP-0x4c` at `0x414F3E55` to the unresolved indirect
call at `0x414F3E58`. Its target identity, ABI, writes and runtime binding remain
unknown; the W6 `0x289A6D60` frontier and distinct client candidates are unchanged.
The API takes an **externally observed post-return snapshot**, never a fabricated
provider output. Existence of local fields does not establish provider semantics
or prove the original path can be reached.

## Exact minimal raw layout

`GeometryPostCallBytes` is a 0x21-byte span starting at original `EBP-0x4c`:

| Offset | Bytes | Observed use |
|---|---:|---|
| 0x00 | 4 | Little-endian binary32 scalar (`EBP-0x4c`) |
| 0x04 | 16 | Opaque payload (`EBP-0x48`) |
| 0x14 | 8 | Opaque payload (`EBP-0x38`) |
| 0x1c | 4 | Unread gap; no field identity asserted |
| 0x20 | 1 | Byte tested for nonzero (`EBP-0x2c`) |

`GeometryOutputBytes` is the minimal 0x38-byte span at original `EDI`, loaded
from `[EBP+0x14]` at `0x414F3DD0`. It is not a complete original structure.
Output binary32 `+0x2c` must already be observed/initialized. The only writes are:

1. `UCOMISS 1.0,scalar; JBE` rejects equality, greater or unordered values;
   return is -1 and **no output byte changes**. There is no nonnegative guard:
   negative values, negative infinity and both zeros take the other arm.
2. Multiply `scalar * old_output[+0x2c]` using scalar SSE, then store `+0x2c`.
3. If observation `+0x20 != 0`, store byte 1 at output `+0x37`. Otherwise leave
   that byte unchanged; this is not a boolean assignment or a clear.
4. Copy 16 opaque bytes `observation+4 -> output+0x0c`, then 8 bytes
   `observation+0x14 -> output+0x1c`.
5. Return 0 (original ESI is cleared and later copied to EAX).

The original epilogue's cookie check must succeed; it is outside this API.
The input/output snapshots must be disjoint, stable and accessible, with no
concurrent observer. There is no general aliasing, ownership or original object
size claim. In particular output `+0x37` is **not proved to be Point+26**.

## Floating-point contract and checks

This offline C++17 module requires x86 SSE and IEC-559 binary32. It uses UCOMISS
and MULSS intrinsics, not a generic geometry substitute. SSE operand order,
NaN handling, signed zero and current MXCSR rounding/FTZ/DAZ behavior are retained.
For the tested result/store contract, exceptions are masked; unmasked FP traps
and their partial-store behavior are not covered. The API does not change MXCSR
control bits, but source operations can set its exception status flags.

Tests use nearest-even, FTZ/DAZ disabled and masked exceptions, restoring the
previous MXCSR afterwards. They check the exact whole output span, untouched
input, threshold neighbors, positive infinity, negative scalars/infinity,
quiet/signaling NaNs (including UCOMISS invalid status), signed zeros, a subnormal,
NaN payload preservation/quieting in multiplication, zero/nonzero (including high-bit) byte cases,
repeated multiplication, sticky-byte preservation and enclosing canaries.
Build flags retain `-fno-fast-math -ffp-contract=off`. No original function was
executed to produce expected results. Tests prove this bounded implementation,
not original provider reachability or full hitbox/capsule/position coverage.

From the worktree root:

```sh
make -C continuation BUILD=../runs/w8-geometry-build \
  ../runs/w8-geometry-build/test_geometry_slice \
  ../runs/w8-geometry-build/test_geometry_slice_sanitize
runs/w8-geometry-build/test_geometry_slice
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  runs/w8-geometry-build/test_geometry_slice_sanitize
python3 -B continuation/evidence/wave8/geometry/verify_source_bytes.py \
  --handoff /tmp/hoplite/workspace/runs/handoff/RAGEBOT_HANDOFF_V10
```

The verifier uses the ZIP and DLL directly and needs no extracted `workspace/work`
file. It hashes both inputs, compares the resource identity, checks every window
and the threshold, and independently decodes 23 selected instructions with
`objdump` using temporary byte windows. Original inputs/work and all old evidence
are read-only. The coordinator still owns integrated `make check` and independent
fixed-SHA acceptance; no common Makefile/status/docs or damage files are changed.

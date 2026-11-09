# W7 reset/history fixed-SHA review

## Scope and disposition

Independent review of the integrated reset/history slices at fixed code SHA
`562d1c78f2df1343cf9a9e3e3f91a115be2069ac` in worktree
`/tmp/hoplite/232-w7-review-state`. No implementation or prior evidence was
modified. Bounded acceptance: no blocker found in the reviewed contracts or
source-backed local behavior. This is not full-suite acceptance or GAP/language/
ABI closure.

Integrated follow-ups are present: reset body snapshot change
`474c3ed7dc4e1ba5bd063ea5ed79a43c0c5dccda`, followed by history cursor and
post-call-observation correction
`f28f7e1a86038d44a27b4df1c4f3f077fffb673a`. The current review head also
registers the W7 source gates.

## Source pins and findings

The read-only handoff image
`workspace/work/nl_handoff_2026-11-06/nl.bin` is SHA-256
`682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e`.
The history gate independently confirmed that image matches the resource at
DLL offset `0x629B10` in `neverlose.dll` (SHA-256
`c1ce8c6d6f1172114d3cff6efe147903292be5eaa6e1c2e28495440fa8f7982c`).
No original binary was loaded, executed, or emulated.

- **Reset body and predecessor:** pinned decoded range
  `0x415DEBF0..0x415DECB9` contains 25 DWORD stores of `0xffffffff` at
  offsets `0,8,...,0xC0`; the `+4..+7` gaps and `0xC4..0xC7` tail are
  untouched. The predecessor at `0x415DECB9..0x415DECC9` adds `0xC8`,
  compares against `0x4250309C`, and jumps back to `0x415DEBF0` unless
  equal. The C++ unsigned-32-bit predicate and raw `0xC8`-byte copy API
  match this local behavior; the original snapshot remains unchanged.
- **Reset fallthrough:** `0x415DECC9..0x415DECEE` has three DWORD stores in
  order (`0x424FDDA8=FFFFFFFF`, `0x424F4B7C=0`,
  `0x425008CC=FFFFFFFF`), then one byte store
  (`0x425009E9=0`). The existing 11-store local reset begins at
  `0x415DECEE`. The implementation separates raw record application from
  metadata tracing and does not infer the EAX provider or record identity.
- **History cursor/index:** source body
  `0x413F88F0..0x413F89B0`, SHA-256
  `b8d8206f38ea7844a1db06850dd6d6a098b2fc0e989946a045ca26ecff200a92`,
  confirms an unsigned `field_08 > field_10 + 1` no-growth branch, the
  intermediate masked cursor store at `0x413F8914`, `DEC EDI` at
  `0x413F891A`, distinct table index `EBX = EBX & EDI` at
  `0x413F891B`, and final unmasked cursor store at `0x413F899A`.
  Capacity 5 / old cursor 4 therefore gives cursor 3 and selected index 0;
  the corrected model and regression test assert both. The DWORD increment
  wrap is also tested.
- **History storage and observation boundaries:** initialization matches the
  ordered source clears and vector at `0x4205AFD0`; the offline model
  explicitly requires a non-null, 16-byte-aligned record of at least
  `0xBC` bytes and stable pairwise non-aliasing among its view descriptor,
  entry-descriptor array, and selected record. It does not claim recovered
  object layout or ownership. Growth/allocation branches stop unresolved.
  Caller bytes at `0x413F70F3/F8/FD` and `0x413F7102/0A` support a separate
  post-provider slot observation; the adapter does not assume the opaque
  `0x41B70930` call preserves or changes the pre-call slot. Missing
  observations return without copying. The shared consumer stages each DWORD
  through a local word, retaining ordered load/store behavior.
- **Remaining frontier:** EAX/table provenance, reset scheduling/field
  meanings, growth and allocator helpers, slot lifetime/ownership, opaque
  provider effects, and full history selection remain unknown. No GAP,
  original-language, ABI, or end-to-end behavior claim follows from these
  local matches.

Key evidence hashes: reset range manifest
`continuation/evidence/wave7/reset/reset_projection_evidence.json`
(`cffa20c523abf58fafc5b083ede5d38fce1dcfaa948e4707f49e59f28ffce830`);
reset objdump
`continuation/evidence/wave7/reset/reset_projection_objdump.txt`
(`f6da31761ead3d6325ce5ae1a85bfd2febc3d4275b75577bebf91cd949780bc6`);
history disassembly
`continuation/evidence/wave7/history/source_disassembly.txt`
(`1d8ad95e91a3ff8eff24610f17da7ace493d6c2bfae8cd029cf83f7c086c45e0`).

## Reviewed code/test pins

SHA-256: reset header `9d8524a003c63b4b5c6f442c27ab938e9a8a308e41f01c81270f89afe074021f`,
source `350c321a249353f4f6c1e09f652b3101aaa62679808a74e367836a08efea66f6`,
test `2ede9bf6eeb6e69e9168a2abae29c44e4835f6ca2c22de572e433ecfe87fc14a`;
history header `82ef05fb682b773ca282a5443d03748035d7e36837cbca546801769f4f5c9b1a`,
source `261320bd5c8dcec59eeda0436bfb5402c50e44f26cd30d4d09cca70cd7667d01`,
test `1bb1fbbf6988c45cbfcd0e7eeb4b131f4d7bb6123467c19142b09c864bdd4ff6`.

## Verification

Detailed fresh outputs and exit codes are in the adjacent `*-byte-gate.log`,
`focused-build-*.log`, and native/sanitizer logs. Focused targets were built in
`/tmp/hoplite/w7-review-state-build` with C++17, warnings-as-errors,
`-fno-fast-math -ffp-contract=off`; both native tests and both ASan/UBSan
tests passed. `test_history_slot_slice` reported 55 assertions in each mode.
Both byte gates passed against the read-only handoff. Tools: g++ 13.3.0,
Python 3.12.3, GNU Make 4.3, GNU objdump 2.42.

The coordinator-owned full suite was not run here. Parent should run it after
integration; no managed browser check applies to this bounded non-UI review.

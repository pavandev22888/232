# W8-RESET: fixed-source six-record reset table

Base: `9b2a75fbd2856fd4966cb6bf6c4933077c51bf41`. Static analysis only.
The new anchor is `MOV EAX,0x42502BEC` at `0x415DEBDE`, immediately
followed by NOP padding and the accepted body at `0x415DEBF0`.
This resolves the local EAX source for this entry path; it does not prove
that every possible entry into the body uses this source.

The body writes 25 DWORDs of `0xffffffff` at offsets `0,8,...,0xc0`,
adds `0xc8` to EAX, compares against `0x4250309c`, and repeats on inequality.
The difference is exactly `6 * 0xc8`: six records, 150 ordered DWORD stores.
Each four-byte gap, including `+0xc4..+0xc7`, is untouched. Equality leads
directly to the four W6 fallthrough stores and eleven W5 charge stores.
Those accepted helpers are reused, not replaced with guessed providers.

`apply_reset_table_snapshot` applies this local chain to six raw records
and the existing separate global projection. Inputs are immutable snapshots
of distinct valid destinations. The returned aggregate is an offline API,
not an assertion about original object layout, atomicity or rollback.
No other thread, exception or observer interleaves the modeled stores.
The static addressing convention is VA minus `0x412a0000`, not a proven
live image base. `anchors.json` pins the complete source and new windows;
the verifier independently constructs all 25 instruction encodings.

Tests inspect every byte of all six records with zero, all-one and
row-distinct patterns, input preservation, all fifteen global stores,
and idempotence. Native and ASan/UBSan use no fast-math or FP contraction.
Execution commands, exits and local log hashes are in `verification.json`.

Still open: incoming call/entry scheduling, runtime image identity, the
remainder of `0x415DEBD0`, exception/cookie handling, concurrency and the
full DT/HS/reset state machine. This closes only the local table source
and iteration count; GAP03 and FULL01 remain partial/open.

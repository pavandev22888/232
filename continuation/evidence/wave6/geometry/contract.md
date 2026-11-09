# W6 geometry caller: evidence frontier

Status: blocked at the external target binding. This result records the caller-side evidence only; it does not add a substitute geometry provider or claim a complete capsule routine.

## Inputs and provenance

Base: 180616d1227235fc952ad07202cdd7529c0272e8. The read-only handoff assets were hashed during this pass:

- workspace/input/nl_handoff_2026-11-06.zip, SHA-256 cc7fcb0e3a34d1f5b3adf884e889e9458c02158ee9823a0c45e161786920dd49; its nl.bin entry is 55,578,624 bytes, SHA-256 682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e.
- workspace/input/dll/client.dll, SHA-256 183d42fac8a4ecc24dc02edf5f09122f37fe9e7533ca093a867718b25cfdeb03; PE32/i386, preferred image base 0x10000000.

nl.bin address mapping uses the prior conditional VA = file offset + 0x412A0000 convention; this is not evidence of a live module base. Fresh objdump 2.42 output and exact byte windows are saved in objdump.txt and anchors.json. The source assets were read only; no DLL or game code was loaded, executed, emulated, or injected.

## Caller-side facts

At the conditional address 0x414F3C90 (file offset 0x253C90), the routine reads four 32-bit stack slots at [ebp+8], [ebp+0xc], [ebp+0x10], [ebp+0x14]. The first three are used as pointer-like records at the offsets listed in anchors.json; the fourth is used as an output base. The byte evidence does not establish named C++ types, full record lengths, aliasing, field meanings, or the original calling convention.

The caller performs scalar/SIMD local calculations, initializes [ebp-0x4c] to binary32 1.0, derives an indirect target from the immediate/rotate/xor/sub sequence, then calls eax. Fresh 32-bit arithmetic gives 0x289A6D60. At the call, edx retains the incoming [ebp+8] value, ecx points at [ebp-0x4c], and stack slots hold pointers to [ebp-0x58] and [ebp-0x24] plus an xmm2 scalar. These are observed register/stack values, not a proved callee signature.

After the call, the code compares the local scalar at [ebp-0x4c] against a binary32 1.0 at 0x4205A554. UCOMISS followed by JBE takes the failure path for values greater than/equal to 1.0 and for unordered values. On the ordered less-than path, the caller multiplies output +0x2c by that local scalar; conditionally writes 1 to output +0x37 only when [ebp-0x2c] is nonzero (it does not clear the byte otherwise); copies 16 bytes to output +0x0c and 8 bytes to +0x1c; and returns 0. The failure result remains -1 and those output writes are skipped. The field identity at +0x37 is not established and is not Point+26 by evidence in this slice.

This is enough to document the local caller effects, but not to implement the call faithfully: both the callee identity and its ABI/data contract are unresolved. The label "capsule caller" remains a scope label, not a fresh proof of the provider's geometry semantics.

## Candidate distinction and stop point

The named preferred client addresses remain separate:

| Candidate | Fresh byte-level observation | Required hypothetical image base to place its RVA at 0x289A6D60 |
|---|---|---:|
| 0x10876D60 (RVA 0x876D60) | begins with a conventional EBP prologue and vector-float body; its argument/register shape is structurally plausible | 0x28130000 |
| 0x103E6D60 (RVA 0x3E6D60) | begins in an EBP-based block with loop/control-flow bytes; a standalone function entry is not established | 0x285C0000 |

The same computed VA and PE preferred base do not establish either runtime base. Neither candidate is bound to the caller, and the second is not merged with the first. I stopped at the first specific unresolved frontier: provider identity/runtime binding. No other archived geometry alternatives were searched.

## Result and verification

No C++ header, implementation, or geometry unit test was added. Doing so here would either invent the target provider/signature or create a test-only stand-in, contrary to the evidence boundary. The byte verifier checks the pinned inputs, function/call/effect windows, target arithmetic, threshold bytes, PE sections, and both separate candidate windows:

    python3 -B continuation/evidence/wave6/geometry/verify_geometry.py --handoff /tmp/hoplite/workspace/runs/handoff/RAGEBOT_HANDOFF_V10

The precise remaining proof needed is a trustworthy static binding of 0x289A6D60 to a module RVA/runtime image base and the callee ABI/data contract. Without it, geometry semantics, capsule applicability, all hitbox/state coverage, and output-field identity remain open.

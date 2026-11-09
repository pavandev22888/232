# W7-DAMAGE: one conditional hitgroup-3 arm

This slice models only the `0x41370710` hitgroup-3 arithmetic arm. Its observed input is the binary32 result of the unresolved indirect call at `0x413708da`; for hitgroup `3`, the body multiplies that value by binary32 `1.25`, then joins the common float32 store/x87 return at `0x41370a98..0x41370ab0`. The caller stores the returned value at `[esi+0x148]` (`0x4136f0f5`). The original third stack argument is a float32 on other arms, but this selected arm does not read it.

The source pins are in `source_anchors.json`. They bind the raw resource to `neverlose.dll` by SHA-256 and embedded-resource offset, check all recorded bytes, decode selected instructions with `objdump`, and verify the switch table entry and constant. Run:

```sh
python3 -B continuation/evidence/wave7/damage/verify_source_bytes.py \
  --handoff /tmp/hoplite/workspace/runs/handoff/RAGEBOT_HANDOFF_V10
```

The `0x4136ffa0` helper is reached by the saved callsite at `0x4136e09e` and calls a separate unresolved trace interface at `[vtable+0x14]` (`0x413700e6`). The trace-buffer layout and any dataflow from that output to the later `0x41370710` call are not established here. Also unresolved are the `0x413708da` provider object's identity/effects, initialization and guard feasibility, other hitgroups, armor, and the full weapon/surface damage path. No provider defaults, trace model, or full-damage API are supplied. The C++ function returns `nullopt` for every unmodeled hitgroup; that is an explicit model boundary, not the original function's result.

All source checks read saved bytes and disassemble them statically; no original DLL, game, network, or injected process is loaded or run.

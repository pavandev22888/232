# NEXT-REVIEW-CALLBACK — fixed-SHA independent review

- Reviewed commit: `35b8ada917cdfa018d0f259b1e930d9bc44f5e07`
- Comparison: `180616d1227235fc952ad07202cdd7529c0272e8..35b8ada917cdfa018d0f259b1e930d9bc44f5e07`
- Worktree: `/tmp/hoplite/232-review-callback`, branch `hoplite/sikyon-46eee495--review-callback`
- Result: **BLOCKED — one behavioral mismatch in the callback prefix model.** No implementation or Makefile changes made by this reviewer.

## Blocking finding

The special branch `owner.field_c0 != 0 && accumulator_06 == 0 && accumulator_07 != 0` does not match the bytes at `0x4149E8F0..0x4149E8EE`. In `continuation/src/bounded_aggregation.cpp:90-95`, the model applies `difference_gate`, synthesizes CL as a boolean, and returns a score on its fallback. The decoded path instead tests the incoming CL at `0x4149E8F0`; zero bypasses settings (covered by the earlier zero-CL fast path), while nonzero proceeds to settings. At `0x4149E8AB..0x4149E8AE`, bit 8 branches to `0x4149E8EB`, which restores the saved incoming CL (`mov ecx,[esp]`) and jumps to the score tail. This path does not read owner+`0xA0` or point+`0x30`, so `difference_gate` must not affect it.

When bit 8 is clear, `0x4149E8B0..0x4149E8C9` returns at `0x4149EB28` unless kind is 1, flag 4 is set, and point+`0x25` is nonzero. With those gates met, `0x4149E8CF..0x4149E8E6` scores using the saved incoming CL when owner+`0x138` is zero or signed point+`0x28` is at least 3; owner+`0x138` nonzero with point+`0x28` less than 3 returns without the sum/tail stores. The implementation currently reverses that eligible subcase into `score(1)` and clears CL in cases that score, and scores `1` instead of returning when the three gates fail. Tests at `continuation/tests/test_bounded_aggregation.cpp:172-176` encode the model's result (always score) rather than the decoded branch outcomes.

Please correct this branch and its tests before acceptance, then rerun review/checks on the new fixed SHA. The other accumulator-count-zero bypass (`0x4149E8F0` with incoming CL zero) is modeled as score with CL zero; the unconditional counter increment at `0x4149E797` remains before both return/unknown paths.

## Other scoped review findings

- Freshly verified image SHA-256 `682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e` and source-walk JSON SHA-256 `a37ae4a208ddd886ba402f26b93542e50525ca6ac1dfc0057bb5cc9030be194e`; the static verifier freshly decoded all 175 pinned instructions.
- The other reviewed branch mappings are consistent with their pinned local windows, including wrapped DWORD SUB followed by signed comparison at `0x4149E81E..0x4149E82A` / `0x4149E933..0x4149E943`, low-byte CL handling, and ordered UCOMISS / MAXSS-second-operand behavior. Native and sanitizer regressions pass, but they do not override the blocking mismatch above.
- Settings remain optional observations, not a proven runtime provider. The evidence does not infer object identity from offsets. Initial-point copying is limited to the observed C0==0 branch at `0x4149E9BF..0x4149EA33`, including the 0x50-byte copy and subsequent C0=1 store; nonzero-C0 ranking remains unresolved.
- This is a bounded offline reconstruction only. Original language/ABI, provider bindings, unknown-index handler semantics, full callback/ranking, full GAP and external audit are not established. No original binary was loaded, executed, or emulated.

## Fresh checks

Exact commands, tool versions, output and exit codes are in [`verification.log`](verification.log). Static evidence, native test, sanitizer test, and reviewed diff whitespace check each exited 0 on the fixed SHA. The tests reported 16,392 assertions per native/sanitizer run.

## Acceptance and next step

Acceptance: **blocked**, not ready. Fix the special `C0!=0, accumulator+6==0, accumulator+7!=0` branch and replace its self-referential expectations with branch-derived tests. Keep review scoped to the fixed SHA; separately review all new code at one fixed integrated SHA when the coordinator requests final integration review.

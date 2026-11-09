# W5-REVIEW-PROVIDERS — ограниченная независимая приёмка

**Результат: bounded acceptance; блокирующих замечаний к новым утверждениям нет.**
Проверен интегрированный SHA `f2003a7c47d58be6e07c8a9eb2bf7ffe9abfc65a`,
diff-base `3e5fd5f8f1ac3dadd635d19ed8b946ee2567fd3b`.
Это агентное ревью, не внешний аудит и не закрытие R03/R04/GAP04/GAP06.

Worktree: `/tmp/hoplite/232-w5-review-providers`. Назначение W5-REVIEW-PROVIDERS
сверено с `/tmp/hoplite/workspace/runs/wave5/reviews.json`; прочитаны AGENTS.md
и контекст проекта; вложенных AGENTS/CLAUDE не обнаружено. Область — ровно
`continuation/evidence/wave5/{esi,indirect,fallback24,p-tolerance,historical}`.
Общие status/docs намеренно ожидают refresh координатором, не считаются новыми
claims пакетов. Авторские логи не заменяли свежие запуски.

## Принятые утверждения и обязательные границы

1. **ESI — условное сохранение, не полный ABI-proof.**
   `esi/preservation.json:238–271`: entry ESP=S, saved ESI лежит в
   S−0x10=B−0x0c до alignment; ESI затем становится локальной базой L.
   57 прямых DWORD stores имеют extent [0,0xac), ниже saved slot при
   L+0xb0≤S−0x10; это не исключает другие aliases. Эпилог
   `0x41c5c70f..0x41c5c716` восстанавливает ESI при EBP=B и целой saved slot;
   сам этот локальный вывод не требует сохранения текущего ESI вложенными
   вызовами. Entry→return требует normal return/stack discipline всех 9 opaque
   calls, нужные EBP/ESI, отсутствие clobber/fault/exception/non-local обхода.
   Свежий decode: 383 инструкции, один local branch, один supported ret 4.
   Пять unit-тестов проверяют evidence/rejection, не поведение оригинала.
2. **INDIRECT — только snapshot-dispatch.** `indirect/contract.md:15–62`:
   raw bias `0x412a0000` не runtime base/RVA binding. Snapshot DWORD
   `[0x4226723c]=0x41c683a0`, JMP rel32→`0x42032493` подтверждены.
   Runtime contents/loader/import identity не установлены; `push 0x4940`
   не доказывает allocator, размер выделения, тип, EAX или lifetime.
3. **+0x24 — один кандидат, не provider.**
   `fallback24/initializer.json:438–545`: immediate `0x3c888889` и
   `8+0x1c=0x24` подтверждены. S_store=S_entry через `0x4200cf20`,
   V_install=A и P_read=V_install — отдельные conditional joins.
   W5-ESI даёт лишь условный аргумент для внешнего вызова, не снимает вложенный
   контракт или T1–T8. Нужны field stability, normal flow, нужная zero-compare
   публикация, отсутствие подмены/ABA, один live instance, lifetime/extent,
   ordering/visibility и отдельное обоснование каждого read.
4. **P/tolerance — корректный prerequisite stop.**
   `p-tolerance/frontier.json:28–36,230–254`: нового writer/supplier anchor
   в перечисленной области не предъявлено. Свежие W3/W2 checks подтверждают
   прежних consumers, не отсутствие writer вообще. Ячейка `0x4223ca04`
   не R03 `0x4252194c`; P_write/P_age/каждый P_tolerance независимы без
   identity/lifetime/noninterference evidence. P+0 writer сам по себе не
   supplier tolerance P+0x14. Девять source pins и два assignment pins
   проверены; последние — именно на объявленном старом SHA.
5. **H01 — новый pefile leaf, не original producer/protocol.**
   `historical/replay_pefile_leaf.py:33–51` проверяет source hash pefile до
   импорта, hashes двух входов, передаёт DLL как bytes, выбирает resource
   directory. В своей ignored deps установлен один pure-Python pefile
   2024.8.26 по hash-pinned requirements; native package files нет.
   Fresh output полностью совпал с `historical/leaf.json`: BINARY/101,
   language 1049, raw offset 0x629b10, size 0x3501000, resource SHA256
   `682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e`.
   Совпадение предиката №5 не восстанавливает неизвестный старый producer.

## H01: все 12 historical-only пунктов

Сверены ordinal, имя и полный predicate с original metrics JSON и
`wave2/gap06/dispositions.json`. Все 47 dispositions byte-identical diff-base;
12 entries имеют конкретные gate/prerequisites/remaining. Источники —
`historical/frontier.json:8–281`, metrics `#/checks/{ordinal−1}`.

| № | Старый gate | Принятая граница |
|---|---|---|
| 5 | pefile 2024.8.26 resource predicate | Свежий leaf PASS; original producer не запускался |
| 6 | LIEF 1.0.0-d05b3499b resource/hash | Не запускался; нужны named parser/build/API и свой replay |
| 199 | Rizin 0.9.1 resource, 143 anchors | Не запускался; нужны executable и точная invocation/base/options |
| 200 | Rizin 0.9.1 client, 19 anchors | Не запускался; свой input и точный протокол |
| 202 | VEX reload_literal_read, 45 bytes/14 instructions | Нет pyvex/archinfo 10.0.5; нужны точные VA/bytes/lift options |
| 203 | VEX height_clamp, 24/4 | Тот же dependency/input frontier, свой span |
| 204 | VEX height_x87_conversion, 28/5 | Тот же frontier; IR не доказывает runtime rounding |
| 205 | VEX metric_compare, 21/4 | Тот же frontier, свой span |
| 206 | VEX strict_threshold, 29/6 | Тот же frontier, свой span |
| 207 | neverlose.dll unchanged after old audit | Нужны original interval hashes/manifests/provenance; нынешний hash не замена |
| 208 | client.dll unchanged after old audit | То же для client.dll |
| 209 | engine.dll unchanged after old audit | То же для engine.dll |

Anchor inventory действительно содержит 143 resource/19 client records;
цитируемые VEX records — только labels/counts, без VA/bytes/options.
LIEF/pyvex/archinfo и rizin не обнаружены в среде ревью, не устанавливались.
Ledger H01 имеет `script_present=False`; проверено отсутствие лишь трёх
названных producer paths, не всего дерева. Strict old protocol, all-209
replay и независимый внешний аудит остаются открыты.

## Свежие команды и exit codes

Все команды выполнены из worktree выше. Только здесь для сокращения:
`H=/tmp/hoplite/workspace/runs/handoff/RAGEBOT_HANDOFF_V10`,
`R=/tmp/hoplite/232-w5-review-providers/runs/review-providers`.
Раскрытые команды, версии, размеры и SHA256 stdout/stderr — `acceptance.json`;
просмотренные очищенные excerpts — `verification.log`.

| Команда | Exit |
|---|---:|
| `python3 -B continuation/evidence/wave5/esi/verify_preservation.py --handoff "$H"` | 0 |
| `python3 -B continuation/evidence/wave5/indirect/verify_indirect.py --handoff "$H"` | 0 |
| `python3 -B continuation/evidence/wave5/fallback24/verify_initializer.py --handoff "$H"` | 0 |
| `python3 -B continuation/evidence/wave3/source/verify_source.py --handoff "$H"` | 0 |
| `python3 -B continuation/evidence/wave2/gap04/verify_scalar_helper.py --handoff "$H"` | 0 |
| `python3 -B -m unittest discover -s continuation/evidence/wave5/esi -p 'test_*.py' -v` | 0, 5 tests |
| `python3 -m pip install --only-binary=:all: --require-hashes --no-deps --no-compile --target "$R/deps" -r continuation/evidence/wave5/historical/requirements.txt` | 0 |
| `PYTHONPATH="$R/deps" python3 -B continuation/evidence/wave5/historical/replay_pefile_leaf.py --handoff "$H"` | 0 |
| `python3 -B runs/review-providers/audit_pins.py` — локальная bounded сверка pins/12 predicates/immutability, hash в acceptance | 0 |
| `git diff --check` | 0 |
| `git diff --cached --check` | первый packaging run 2; после исправления EOF 0 |

Первый staged whitespace check обнаружил только лишнюю пустую строку EOF
в новом очищенном логе ревью. Она удалена; первый диагностический лог сохранён
отдельно, исправленный staged check прошёл. Проверки исследуемых пакетов не падали.

## Неизменность и остаток

За интервал свежих запусков совпали size/mtime/SHA256 **28 закреплённых
source/input/archive файлов**, включая шесть ZIP. Это scoped immutability,
не whole-HANDOFF аудит и не исторические интервалы №207–209.
HANDOFF/input/work/архивы не изменялись; original DLL execution/loading/
emulation, игра, injection не выполнялись. Старые скрипты, пишущие в HANDOFF,
не запускались. Рабочий audit helper и полные логи остались в собственном runs.

Новых C++/provider/API здесь нет. Полный make/native/sanitizers намеренно
не повторялись по назначению: это code-review/координаторский интеграционный
gate, не эта evidence-only приёмка. UI отсутствует; browser verification
неприменима. Общие docs/status/Makefile и archival dispositions не менялись.
Допустимая интеграция отчёта не закрывает unknowns и не утверждает полноту
восстановления. Проверенный SHA — указанный выше, не будущий refresh SHA.

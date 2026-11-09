# W8-PROVIDER — сохранение ESI у непосредственного field-initializer dependency

**Разрешён один локальный prerequisite W5-FALLBACK24:** вызов
`0x41C610B4 → 0x4200CF20` не меняет ESI при поддержанном нормальном возврате.
Поэтому `S_store=S_entry` для записи `0x41C610CA`; прежнее условное
смещение `O_entry+8+0x1C` теперь имеет локально подтверждённую базу.
Это не поставщик runtime-значения и не закрытие R03/GAP04/FULL01.

Работа выполнена в отдельном worktree `/tmp/hoplite/232-w8-provider` от
`9b2a75fbd2856fd4966cb6bf6c4933077c51bf41`, только в этом каталоге evidence.
Общие документы, код, Makefile, старые evidence и handoff не изменялись.
Проверяемые утверждения/предпосылки: [contract.json](contract.json).

## Новые байты и маленький граф

Источники и ZIP-members закреплены полностью в [anchors.json](anchors.json).
`workspace/input/nl_handoff_2026-11-06.zip::nl.bin` по SHA256 совпадает
с handoff `workspace/work/nl_handoff_2026-11-06/nl.bin`:
55 578 624 байта, `682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e`.
`functions.zip::functions/4200CF20.c` прочитан только как подсказка декомпилятора;
его `__cdecl` и тип `__m128i *` не приняты за исходную декларацию.

| Окно, полуоткрытое | Offset | Байты / инструкции | SHA256 |
|---|---|---|---|
| **Новое** `4200CF20..4200D07A` | `D6CF20` | 346 / 89 | `a926212d42a287f420bd22d103ff89eb9f842d37a088c203a60a3d8c9b44c978` |
| Унаследованное `41C5BDA0..41C5BDFA` | `9BBDA0` | 90 / 20 | `3f829c1a2d081fe2efa99cba5c9a2b19003de80918afdcd5079a5c8253939061` |
| Унаследованное `41C610A0..41C610D1` | `9C10A0` | 49 / 14 | `f7f4a0293b1ec98e93d590a830b62ac181791d2bf4a133029684dde75d7777ff` |

Все адреса — **условные raw-метки** `offset + 0x412A0000`, не выбранный
runtime-base, не установленный PE/RVA mapping. Свежий полный декод этих окон:
[source_objdump.txt](source_objdump.txt).

В новом теле 17 прямых jump-рёбер: все ведут на границы инструкций внутри
окна; все fallthrough также остаются внутри. Нет CALL/непрямого перехода.
Три bare RET: `4200CF64`, `4200D024`, `4200D079`. Ни одна из 89 инструкций
не использует ESI/SI явно или как неявный destination; единственный строковый
opcode — `F3 AA` (`REP STOSB`, EDI, не ESI). Следовательно, ни одна поддержанная
нормальная ветвь не меняет ESI. Статическая достижимость всех узлов здесь не
является доказательством реально взятой ветви, завершения циклов или вызова.

Унаследованный caller фиксирует `ESI=entry ECX`, затем PUSH `0x1520`, PUSH 0,
PUSH ECX и direct CALL. Leaf читает DWORD `[ESP+C]`, BYTE `[ESP+8]`, DWORD
`[ESP+4]`; caller очищает 12 байт. Это конкретные операнды данной границы,
не доказательство исходного calling-convention keyword или типа объекта.
Сохранение ESI не зависит от значений двух runtime feature-bit reads
`42523918`/`424AA08C`; сами эти значения не подставлялись.

## Что теперь следует и где остановка

При названных в контракте условиях запись `41C610CA` кладёт **точные четыре
байта DWORD `0x3C888889`** по `S_entry+1C`, то есть `O_entry+24` на
номинированном parent→initializer ребре. Никакой float-арифметики для вывода
не требуется. Это не наблюдавшийся fallback и не фиктивное значение provider.

Остановка на первом непокрытом переходе **после записи** (`41C610D1`): не
доказана сохранность этих четырёх байтов до завершения initializer, остальных
вложенных вызовов parent и публикации `41C5E404`, а затем до отдельного чтения
`41C784D8/DE`. W5-ESI доказал локальный epilogue parent, но его целый
entry→return по-прежнему условен на stack/register/alias-контракты девяти
вложенных вызовов. Новое leaf-доказательство не снимает этих условий.
Indirect provider `[4226723C]`, семантика snapshot thunk-target `42032493`,
writer-before-read, lifetime/ABA, synchronization, mapping и реальная
достижимость остаются отдельными unknown. Ни один из них не заменён default.

W7 ECX/EDX callback и W5 `type_info`-marker относятся к другим локальным
границам: связи с этим объектом/RTTI descriptor не установлено. Эти leaf-байты
не устанавливают исходный язык, стандарт, compiler или whole-project ABI.
Новые широкие сканы, RTTI-инвентари и подбор runtime bases не выполнялись.

## Воспроизведение и приёмка

Из корня worktree:

```sh
python3 -B continuation/evidence/wave8/provider/verify_source_bytes.py \
  --handoff /tmp/hoplite/workspace/runs/handoff/RAGEBOT_HANDOFF_V10
```

Standalone verifier только читает источники/ZIP-members, повторяет SHA256,
побайтовые окна, fresh GNU objdump i386, локальный CFG и два rel32.
Ничего не записывает и не интерпретирует/не исполняет оригинальные инструкции.
Точные свежие команды, версии, exit codes и хеши логов —
[verification.json](verification.json), stdout/stderr — соседние `.log`.
Native/ASan/UBSan не запускались: новый C++-код или модель не добавлялись.
Полный интегрированный `make check`, fixed-SHA review и решение о приёмке —
gate координатора, не объявлены выполненными этим evidence-коммитом.
Оригинальные DLL не загружались/не запускались/не эмулировались; game,
injection, hooks, UI, push и merge не использовались.

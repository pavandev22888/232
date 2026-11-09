# Карта материалов и текущего знания

Принятый ограниченный код W7: `562d1c78f2df1343cf9a9e3e3f91a115be2069ac`; приёмка — в
[W7 verification](../continuation/evidence/wave7/verification.json).
Ниже W6 и более ранние снимки отделены от текущих приращений.
Исторический интегрированный код W6: `d99dd375160c7fccfd489598942232430d78da2f`.
Ограниченные контракты приняты после независимых fixed-SHA ревью; поправка
документации отдельно проверена на `c8a78c93b1f6d7cff7c0d9e4fc05c2dfd8915653`.
Исторические снимки W3–W5/R03 и исходные замечания сохранены.
Актуальные статусы полного остатка — [STATUS.json](../continuation/STATUS.json),
назначения следующей работы — [TASKS.json](TASKS.json). Пути ниже относительно
корня Git, если явно не указан **HANDOFF**. Смещения `+14/+48/+50` здесь hex.

Актуальная цель исправлена пользователем 2026-11-09: оригинальная логика на
исходном языке, без обязательного C17-порта. C++17 ниже — язык нынешней модели,
не установленный стандарт оригинала. Исторические приёмки остаются неизменными.

## Текущий читаемый код

Для каждого модуля есть `.hpp` в `continuation/include/rage/`, `.cpp` в
`continuation/src/` и `test_<module>.cpp` в `continuation/tests/`.
Текущие модули — **C++17**, с явно переданными внешними observations.

| Модуль | Реализованный ограниченный контракт | Доказательства/контракт |
|---|---|---|
| [position_records](../continuation/src/position_records.cpp) | N размером 0x74, 2048 записей; update/snapshot, reader, item gate | [SOURCE_MAP](../continuation/evidence/SOURCE_MAP.md), [anchors](../continuation/evidence/byte_anchors.json) |
| [charge_state](../continuation/src/charge_state.cpp) | capacity, raw-byte branch, latch/counter gate, проекция reset трёх полей и отдельный 11-store snapshot | [conditions](../continuation/evidence/gap03/condition_table.md), [reset](../continuation/evidence/wave2/gap03/reset_slice_contract.md) |
| [position_lifecycle](../continuation/src/position_lifecycle.cpp) | N+48 producer, условный actor-consumer, optional N+50 output | [lifecycle](../continuation/evidence/gap05/position_lifecycle.json), [optional output](../continuation/evidence/wave2/gap05/README.md) |
| [resolved_scalar_helper](../continuation/src/resolved_scalar_helper.cpp) | float-helper 0x41C784B0 с явным fallback | [scalar contract](../continuation/evidence/wave2/gap04/contract.md) |
| [marker_pipeline](../continuation/src/marker_pipeline.cpp) | N+50/+54 helper calls → N+48 producer | [composition](../continuation/evidence/wave3/marker/composition.json), [W3 spec](../continuation/SPEC_NEXT.md) |
| [actor_scalar_pipeline](../continuation/src/actor_scalar_pipeline.cpp) | независимые X/Y/Z observations; pure resolver, не решает guards | [provenance](../continuation/evidence/wave3/actor/provenance.json) |
| [bounded_aggregation](../continuation/src/bounded_aggregation.cpp) | Direct/non-direct score-prefix, T+F0; tail statistics, initial copy и normal ranking/replacement tail | [W5](../continuation/evidence/wave5/aggregate/contract.json), [W6](../continuation/evidence/wave6/aggregate/contract.json), [W7](../continuation/evidence/wave7/rank/contract.json) |
| [lifecycle_seed_slice](../continuation/src/lifecycle_seed_slice.cpp) | Quarter-decay seed → существующий marker tail | [опоры](../continuation/evidence/wave5/lifecycle/seed_path.json) |
| [bounded_message_fields](../continuation/src/bounded_message_fields.cpp) | 32/7/32, Z/Y/X, 7 бит; не полный packet/sender | [опоры](../continuation/evidence/wave5/messages/byte_anchors.json) |
| [release_state_slice](../continuation/src/release_state_slice.cpp) | Отдельные post-call observations, latch stores и условный DWORD increment; не автомат DT/HS | [контракт W6](../continuation/evidence/wave6/modes/contract.json) |
| [reset_predecessor](../continuation/src/reset_predecessor.cpp) | 25 DWORD stores в raw 0xC8 snapshot; ADD/CMP/JNE, четыре fallthrough stores и прежняя 11-store проекция; не полный цикл | [W6](../continuation/evidence/wave6/reset/reset_predecessor_evidence.json), [W7](../continuation/evidence/wave7/reset/reset_projection_evidence.json) |
| [history_slice](../continuation/src/history_slice.cpp) | 21 ordered raw-word copy, DWORD observation и zero store после неизвестного вызова; не selector/matrix path | [контракт W6](../continuation/evidence/wave6/history/README.md) |
| [history_slot_slice](../continuation/src/history_slot_slice.cpp) | No-growth existing-slot resolver, отдельные cursor/index, init и независимо наблюдаемый post-call slot | [W7](../continuation/evidence/wave7/history/README.md) |
| [movement_slice](../continuation/src/movement_slice.cpp) | Caller selector/bit0 gate, raw float argument и наблюдаемый AL; не тело helper | [W7](../continuation/evidence/wave7/movement/receipt.json) |
| [damage_slice](../continuation/src/damage_slice.cpp) | Только hitgroup 3: observed provider float × 1.25f; не trace/armor/provider | [W7](../continuation/evidence/wave7/damage/README.md) |

Дополнительный `test_position_pipeline.cpp` проверяет связки. Успешные тесты
не устанавливают внешних providers и исходное расписание вызовов. Транзакционное
сохранение N при `unresolved` — соглашение офлайн API, не доказанный rollback оригинала.

## W7: текущая ограниченная интеграция

[Сводка W7](../continuation/evidence/wave7/README.md) фиксирует шесть участков,
исправления cursor/index и reset snapshot, тесты и независимые ревью.
Локальная [ABI-связь](../continuation/evidence/wave7/abi/contract.json)
подтверждает caller ECX/EDX и обычный возврат, не source declaration/язык.
Все runtime providers, полный selector и полные GAP остаются открыты.

## W6: историческая приёмка до W7

Новый [контракт callback](../continuation/evidence/wave6/aggregate/contract.json)
сохраняет старые direct/F0 API и increment до non-direct ветви, не подставляет
settings providers и index handler. Реализованы statistics stores и начальное
копирование Point при `owner+C0==0`; на снимке W6 ranking оставался открыт.
W7 добавляет ограниченный normal tail, не полный selector или его providers.

[release_state_slice](../continuation/src/release_state_slice.cpp) восстанавливает
локальные ветви/записи `415E01A0..415E0201` с отдельными observations после
helper и indirect call: [контракт](../continuation/evidence/wave6/modes/contract.json).
Это не полный автомат DT/HS; сохранность globals через вызовы не предполагается.
Источники, hashes/bytes, решения по ревью и свежие команды — в
[W6 verification](../continuation/evidence/wave6/verification.json). Координаторский
полный gate на `b869125873217d955cf56788219fc7e2879b17f4` прошёл с exit 0.
Проверка окончательного публикуемого SHA фиксируется отдельно в PR #1.

History F1 исправлен: чтение `[ebp-0x38]` — `0x413F71A1`, запись в `+0x3C` —
`0x413F71A4`; `0x413F7102` загружает указатель `[ebp-0x28]`.
[Независимый follow-up](../continuation/evidence/wave6/review-integrated-evidence/followup/report.json)
подтвердил поправку без изменения кода/тестов. Ошибочные callback/overlap замечания
отозваны отдельными resolutions, исходные отчёты не переписаны. Message/geometry
остаются static frontiers без providers; на снимке W6 damage ещё не имел реализации.
В W7 добавлена только отдельная hitgroup-3 ветвь, без связи с trace output.

## Что остаётся открытым

| Полный ID | Состояние | Точная граница / куда смотреть |
|---|---|---|
| CODE01 | partial | Direct/non-direct локальные ветви/T+F0 реализованы; normal ranking tail добавлен в W7; settings bindings, index handler, enumeration и полный выбор цели открыты; [baseline inventory](../continuation/evidence/wave2/code01/README.md) |
| GAP01 | open | Первые положительные источники Point+26, T+F4, G425009BC; [W1](../continuation/evidence/gap01/RESULTS.md), [W2](../continuation/evidence/wave2/gap01/results.json) |
| GAP02 | open | Типизированный T+68 consumer, связь того же T, сравнения, ветви и действия; [inventory](../continuation/evidence/wave2/gap02/README.md) |
| GAP03 | partial | Полный автомат/entry predicate reset, смыслы дополнительных полей, реальное расписание; [reset contract](../continuation/evidence/wave2/gap03/reset_slice_contract.md) |
| GAP04 | open | Fallback/tolerance providers, runtime bindings, entity/weapon resolution, TLS/FS guards; [bindings](../continuation/evidence/gap04/README.md) |
| GAP05 | partial | Upstream seeds, P/P+0, cross-call identity, полный lifecycle, формат сообщений/sender; [N+14 frontier](../continuation/evidence/wave3/source/README.md) |
| GAP06 | partial | 12 historical-only H01, полный протокол и внешний аудит; [dispositions](../continuation/evidence/wave2/gap06/README.md) |

Остаются также непрозрачный dispatch, exceptional paths, handler индекса вне
поддержанного диапазона, остальные режимы/consumers. [Карта W5](../continuation/evidence/wave5/scope/coverage.json) содержит 24 области
исправленной цели; это не полный call graph и не закрытие неизвестных зависимостей.

**Не смешивать объекты:** N+68 — тип снимка N, не порог T+68. Point, T, N и P
не становятся одним объектом из-за совпадающего смещения или глобального адреса.
Локальный writer `0x414F840B` переносит DWORD P+0 в N+14 при явных предпосылках;
это не доказывает происхождение/положительность значения или равенство N между вызовами.
Исследование этой границы не расширять без нового конкретного байтового основания.

## Историческая ограниченная граница R03 до W5

[Контракт](../continuation/evidence/next/scalar-provider/contract.md) и
[verifier](../continuation/evidence/next/scalar-provider/verify_fallback_provider.py)
фиксируют одну pointer-cell цепочку fallback: условный store `0x41C5E404`
в `0x4252194C`, отдельное чтение при `0x41C784D8`, затем float `+0x24`.
767 литеральных совпадений — не CFG и не доказательство отсутствия иных writers.
Сохранение ESI через callee, indirect provider, порядок writer-before-read,
время жизни и источник float не доказаны. **R03 blocked/frontier, GAP04 open**;
новое тело функции Ragebot не добавлено. Свежие проверки и fixed-SHA review:
[verification](../continuation/evidence/next/scalar-provider-review/verification.json),
[acceptance](../continuation/evidence/next/scalar-provider-review/acceptance.json).

## Уточнения W5

[Итоговая таблица](../continuation/evidence/wave5/README.md) разделяет код,
статические условные связи и frontier. ESI save/restore доказан для локального
обычного эпилога при явных nested-call/stack предпосылках; snapshot dispatch
и кандидат инициализации +24 не устанавливают runtime provider, lifetime или
writer-before-read. P/tolerance остаётся blocked. Полный локальный reset означает только
11 локальных zero stores, не полный автомат. Новый pefile leaf не заменяет
исходный H01 producer, остальные 11 gates или внешний аудит.

## Исходный handoff — не потерян и не заменён новыми отчётами

После [восстановления](BOOTSTRAP.md) читать внутри **HANDOFF**:

1. `НАЧНИ_ЗДЕСЬ.md`, `docs/Цель_и_критерии_готовности.md`,
   `docs/Текущее_состояние_V10.md`, `OPEN_TASKS.json` — исходная цель и остаток.
2. `docs/Карта_кода_и_доказательств.md`, `docs/Исправления_и_ловушки.md` —
   архивная навигация и исправления прежних ошибочных выводов.
3. `workspace/input/` — неизменяемые исходные материалы;
   `workspace/work/` — распакованные файлы, включая декомпиляты/бинарные данные.
4. `workspace/work/recovered_nl/recovered_nl/{src,sdk,tests}` — уже имевшиеся
   локальные C-функции: `nl_core.c`, `nl_rage_math.c`, `nl_netvar.c`, частичные
   SDK-раскладки и собственные тесты. Это не полный Ragebot; язык этих файлов
   сам по себе не доказывает исходный язык оригинала.
   Их пределы и хеши: [PLAN](../continuation/PLAN.md), [inventory](../continuation/evidence/inventory.json).
5. `workspace/orchestration/v1/approval_package/Критерии_проверок.csv` —
   старые 24 критерия, уточняемые поздними материалами; не считать все закрытыми.
6. `workspace/orchestration/v6` … `v10`, `handoff_checks/` — исторические
   контракты/проверки; новые результаты запускать через `tools/replay.py` в копиях.

`BUNDLE_MANIFEST.json` и `workspace/analysis/inventory.json` фиксируют целостность;
проверка содержимого и повторяемый bootstrap описаны отдельно. Исторический
промпт остаётся неизменным. Актуальная поправка пользователя отменяет обязательный
C17-порт: нужна оригинальная логика на исходном языке. Текущая цель и протокол
находятся в `docs/GOAL.md` и корневом `AGENTS.md`.

## Приёмки и история

- [W3 verification](../continuation/evidence/wave3/verification.json) и
  [лог](../continuation/evidence/wave3/verification.log): проверенный код
  `52c513e2b83761b6ea2d758483a4d883907b4341`, 19 902 assertions на режим.
- [W4 source/API](../continuation/evidence/wave4/proof/acceptance.json),
  [W4 tests](../continuation/evidence/wave4/tests/acceptance.json): два агентных
  ревью, не внешний аудит. Первый test-run exit 2 и исправленный exit 0 сохранены.
- [Worktree journal](../continuation/ORCHESTRATION_WORKTREES.md): база, коммиты
  исполнителей и интеграции. Старые абсолютные пути — история, не требование к клону.
- [DECISIONS](../continuation/DECISIONS.md): причины ограничений и важные ловушки.
- `PLAN.md`, `PLAN_NEXT.md`, `SPEC_NEXT.md`, `ORCHESTRATION*.md` — планы/журналы
  завершённых этапов. Общий актуальный план теперь [ROADMAP.md](ROADMAP.md).

Старые evidence описывают состояние своего этапа, а не автоматически последнее.
Например, исходный SOURCE_MAP относится только к первому модулю N, а пометка
об открытом optional N+50 в W1 уточнена локальной реализацией W2. Общие GAP
при этом не закрыты. Не переписывайте старую приёмку как будто она проверяла новый код.

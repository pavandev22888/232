# Карта материалов и текущего знания

Снимок базы: `a21c2ac8045617278280de8ffb33f67dbf743c87`, 2026-11-09.
Актуальные статусы полного остатка — [STATUS.json](../continuation/STATUS.json),
назначения следующей работы — [TASKS.json](TASKS.json). Пути ниже относительно
корня Git, если явно не указан **HANDOFF**. Смещения `+14/+48/+50` здесь hex.

## Текущий читаемый код

Для каждого модуля есть `.hpp` в `continuation/include/rage/`, `.cpp` в
`continuation/src/` и `test_<module>.cpp` в `continuation/tests/`.
Все шесть — **C++17**, с явно переданными внешними observations.

| Модуль | Реализованный ограниченный контракт | Доказательства/контракт |
|---|---|---|
| [position_records](../continuation/src/position_records.cpp) | N размером 0x74, 2048 записей; update/snapshot, reader, item gate | [SOURCE_MAP](../continuation/evidence/SOURCE_MAP.md), [anchors](../continuation/evidence/byte_anchors.json) |
| [charge_state](../continuation/src/charge_state.cpp) | capacity, raw-byte branch, latch/counter gate, проекция reset трёх полей | [conditions](../continuation/evidence/gap03/condition_table.md), [reset](../continuation/evidence/wave2/gap03/reset_slice_contract.md) |
| [position_lifecycle](../continuation/src/position_lifecycle.cpp) | N+48 producer, условный actor-consumer, optional N+50 output | [lifecycle](../continuation/evidence/gap05/position_lifecycle.json), [optional output](../continuation/evidence/wave2/gap05/README.md) |
| [resolved_scalar_helper](../continuation/src/resolved_scalar_helper.cpp) | float-helper 0x41C784B0 с явным fallback | [scalar contract](../continuation/evidence/wave2/gap04/contract.md) |
| [marker_pipeline](../continuation/src/marker_pipeline.cpp) | N+50/+54 helper calls → N+48 producer | [composition](../continuation/evidence/wave3/marker/composition.json), [W3 spec](../continuation/SPEC_NEXT.md) |
| [actor_scalar_pipeline](../continuation/src/actor_scalar_pipeline.cpp) | независимые X/Y/Z observations; pure resolver, не решает guards | [provenance](../continuation/evidence/wave3/actor/provenance.json) |

Дополнительный `test_position_pipeline.cpp` проверяет связки. Успешные тесты
не устанавливают внешних providers и исходное расписание вызовов. Транзакционное
сохранение N при `unresolved` — соглашение офлайн API, не доказанный rollback оригинала.

## Что остаётся открытым

| Полный ID | Состояние | Точная граница / куда смотреть |
|---|---|---|
| CODE01 | partial | Агрегация/выбор цели не реализованы; [baseline inventory](../continuation/evidence/wave2/code01/README.md) |
| GAP01 | open | Первые положительные источники Point+26, T+F4, G425009BC; [W1](../continuation/evidence/gap01/RESULTS.md), [W2](../continuation/evidence/wave2/gap01/results.json) |
| GAP02 | open | Типизированный T+68 consumer, связь того же T, сравнения, ветви и действия; [inventory](../continuation/evidence/wave2/gap02/README.md) |
| GAP03 | partial | Полный автомат/entry predicate reset, остальные stores, реальное расписание; [reset contract](../continuation/evidence/wave2/gap03/reset_slice_contract.md) |
| GAP04 | open | Fallback/tolerance providers, runtime bindings, entity/weapon resolution, TLS/FS guards; [bindings](../continuation/evidence/gap04/README.md) |
| GAP05 | partial | Upstream seeds, P/P+0, cross-call identity, полный lifecycle, формат сообщений/sender; [N+14 frontier](../continuation/evidence/wave3/source/README.md) |
| GAP06 | partial | 12 historical-only H01, полный протокол и внешний аудит; [dispositions](../continuation/evidence/wave2/gap06/README.md) |

Остаются также непрозрачный dispatch, exceptional paths, handler индекса вне
поддержанного диапазона, остальные режимы/consumers. Не все области конечной
цели уже имеют восстановленную карту: R01 в новом плане должен её систематизировать.

**Не смешивать объекты:** N+68 — тип снимка N, не порог T+68. Point, T, N и P
не становятся одним объектом из-за совпадающего смещения или глобального адреса.
Локальный writer `0x414F840B` переносит DWORD P+0 в N+14 при явных предпосылках;
это не доказывает происхождение/положительность значения или равенство N между вызовами.
Исследование этой границы не расширять без нового конкретного байтового основания.

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
   SDK-раскладки и собственные тесты. Это не полный Ragebot и не корневой C17-порт.
   Их пределы и хеши: [PLAN](../continuation/PLAN.md), [inventory](../continuation/evidence/inventory.json).
5. `workspace/orchestration/v1/approval_package/Критерии_проверок.csv` —
   старые 24 критерия, уточняемые поздними материалами; не считать все закрытыми.
6. `workspace/orchestration/v6` … `v10`, `handoff_checks/` — исторические
   контракты/проверки; новые результаты запускать через `tools/replay.py` в копиях.

`BUNDLE_MANIFEST.json` и `workspace/analysis/inventory.json` фиксируют целостность;
проверка содержимого и повторяемый bootstrap описаны отдельно. Исторический
промпт остаётся неизменным: его C/C++-цель уточнена до C17, а работа несколькими
агентами уже разрешена; текущий протокол находится в корневом `AGENTS.md`.

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

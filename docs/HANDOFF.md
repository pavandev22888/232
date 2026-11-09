# Передача работы следующему агенту

**Цель — полная оригинальная логика Ragebot на её исходном языке, не новый
типовой бот. Цель не достигнута.** Обязательный C17-порт отменён пользователем;
C++17 — стандарт текущей офлайн-реконструкции, а не установленный язык оригинала.

Для старта без истории чата передайте агенту [готовый промпт](START_NEXT_AGENT.md).
Этот пакет передаёт работу, но сам не назначает следующую волну и не закрывает GAP.

**Обновление W7:** ниже сохранён исторический снимок W5. Текущая интеграция —
[W7 scope](../continuation/evidence/wave7/README.md),
[verification](../continuation/evidence/wave7/verification.json),
[STATUS](../continuation/STATUS.json) и [TASKS](TASKS.json). W7 добавляет normal
ranking/replacement tail, локальную caller ABI-связь, raw reset snapshot,
existing-slot history resolver, movement caller и hitgroup-3 damage arm.
Не начинать их заново по старому списку W5/W6. Исторические W5/W6 PASS не
подтверждают W7; её fixed-SHA gate и ревью фиксируются отдельно.

## 1. Точка передачи и источники истины

- Репозиторий: <https://github.com/pavandev22888/232>.
- Рабочая ветка: `hoplite/sikyon-46eee495`; [PR #1](https://github.com/pavandev22888/232/pull/1).
  **Без merge, merge queue и auto-merge. Не начинать с одного `main`.**
- Снимок результата W5: `897b4bf51b1931fd1b80f178d40b7478ce251959`, 2026-11-09.
  Документы передачи добавляются поверх него; при старте получить актуальный remote HEAD.
- Интегрированный код, проверенный тремя рецензентами:
  `f2003a7c47d58be6e07c8a9eb2bf7ffe9abfc65a`.
  Между ним и снимком W5 менялись только ревью и coordinator-документы.
- [GOAL](GOAL.md) — критерии полного результата; [STATUS](../continuation/STATUS.json)
  — полный остаток; [TASKS](TASKS.json) — назначения/история bounded-карточек;
  [SOURCE_INDEX](SOURCE_INDEX.md) — модули/материалы; [BOOTSTRAP](BOOTSTRAP.md)
  — восстановление; [AGENT_WORKFLOW](AGENT_WORKFLOW.md) — изоляция и приёмка.
- [W5 summary](../continuation/evidence/wave5/README.md),
  [verification](../continuation/evidence/wave5/verification.json) и
  [integration ledger](../continuation/evidence/wave5/verification.json)
  содержат результат каждого исполнителя, исходный и перенесённый commit, пути и проверки.

`done` в TASKS означает **завершение сформулированной локальной карточки**.
`open/partial` в STATUS относится к полной функциональности. Карточка с точным
frontier может быть done, хотя необходимый provider или полный GAP остаётся открыт.
Ни число карточек, ни assertions не являются процентом готовности.

## 2. Что принято и что нельзя начинать заново

### Рабочий код

Девять C++17-модулей с `.hpp`, `.cpp` и тестами в корневой `continuation/`:

| Модуль | Принятый ограниченный результат | Не означает |
|---|---|---|
| `position_records` | N размером 0x74, 2048 записей, update/snapshot/reader/item gate | Полный lifecycle или все consumers |
| `charge_state` | Capacity/latch/counter, прежняя reset-проекция и 11-store snapshot: 3 BYTE, 8 DWORD | Полный reset entry predicate, scheduling, автомат |
| `position_lifecycle` | N+48 producer, условный actor-consumer, optional N+50 | Все seeds, providers и cross-call identity |
| `resolved_scalar_helper` | Локальная float-арифметика с явным fallback observation | Восстановленный runtime provider/tolerance |
| `marker_pipeline` | Scalar helper composition → N+48 | Доказанный rollback оригинала при unresolved |
| `actor_scalar_pipeline` | Три независимые scalar observations X/Y/Z | Разрешённые guards и entity/weapon resolution |
| `bounded_aggregation` | Direct +1C=2/3 score-prefix и отдельный T+F0 finalizer | Остальные ветви, callback tail, ranking, полный selector |
| `lifecycle_seed_slice` | Quarter-decay seed с существующим marker tail | Остальные seed paths и полный lifecycle N |
| `bounded_message_fields` | Ограниченные bit/coordinate fields и локальная consumer-call boundary | Полный packet framing, sender или downstream acceptance |

Контракты/ссылки на исходники — [SOURCE_INDEX](SOURCE_INDEX.md).
Сохранить regression baseline, порядок FP-операций, NaN/±0, ширины/знаковость,
MINSS/MAXSS и явные unknowns. Не переписывать рабочую базу ради отменённого C17.

### Полный реестр карточек на снимке W5

В [TASKS.json](TASKS.json) **31 карточка: 22 done, 9 blocked; ready/in_progress/review — 0**.
Это снимок реестра, не гарантия отсутствия работы в другой среде: перед назначением
сверить live владельцев. Старые `owner`/worktree у done/blocked не означают активную задачу.

| Карточки | Статус и точная граница |
|---|---|
| R01 | done: карта 24 областей, не полный call graph или язык оригинала |
| R05 | done: локальные reset stores, не entry/scheduling |
| V01 | done: supplemental pefile leaf №5 и prerequisites; старый producer/protocol не закрыт |
| W5-SCOPE | done: 24 области, 39 source pins, два ограниченных языковых признака |
| W5-POINT26, W5-TF4, W5-GLOBAL9BC | done: три условные гипотезы; первые положительные источники не доказаны |
| W5-AGGREGATE, W5-CHARGE, W5-LIFECYCLE, W5-MESSAGES | done: четыре кодовых приращения из таблицы выше |
| W5-THRESHOLD | done: один helper-кандидат; typed T+68 consumer не доказан |
| W5-BINDINGS, W5-ESI, W5-INDIRECT, W5-FALLBACK24, W5-GUARDS | done: локальные anchors/условные связи, не runtime-контракты |
| W5-P-TOLERANCE | done: prerequisite stop в указанном scope; не доказательство отсутствия writers |
| W5-HISTORICAL | done: pefile leaf и поэлементный frontier H01 |
| W5-REVIEW-CODE, W5-REVIEW-EVIDENCE, W5-REVIEW-PROVIDERS | done: три bounded fixed-SHA ревью, без blockers; не внешний аудит |
| R02 | blocked: first-positive источники Point+26, T+F4, G425009BC |
| R03 | blocked: nested-call/stack assumptions, runtime binding, identity/lifetime/order fallback |
| R04 | blocked: нет нового P/P+0/tolerance supplier anchor сверх проверенной W3-границы |
| I01 | blocked: не хватает необходимых тел/связей; не запрет интегрировать очередной принятый локальный участок |
| FULL01 | blocked: полный транзитивный контур, язык, протокол эквивалентности и внешний аудит |
| C01–C04 | blocked/superseded: ошибочная обязательная C17-миграция отменена; **не возобновлять как незакрытый порт** |

Архивные V1–V10, W1–W4 и прежний R03 сохранены. Их PASS/`acceptance_remaining`
и W5 scope-карта — исторические снимки. В частности, текст карты о «шести модулях»
или «не реализованной агрегации» предшествует интеграции W5; актуальные девять модулей
и частичная агрегация указаны выше. Не переписывать архивное evidence задним числом.

## 3. Полные незакрытые задачи

| ID | Статус | Что ещё нужно доказать/реализовать для закрытия |
|---|---|---|
| CODE01 | partial | Non-direct callback, tail/best-point updates, все сравнения/ветви selector, enumeration, T+E8/+EC, ordering и реальные consumers |
| GAP01 | open | Первые положительные источники Point+26, T+F4, G425009BC; происхождение значений, identity, reachability и writer-before-reader |
| GAP02 | open | Тот же T → typed read T+68 → сравнение → ветвь → действие; N+68 не подходит |
| GAP03 | partial | Reset entry predicate, смысл дополнительных полей, другие writers, lifecycle, reset/charge/release scheduling |
| GAP04 | open | Fallback/tolerance, entity и weapon providers, runtime/module binding, nested calls, guards, lifetime и visibility |
| GAP05 | partial | Остальные N+48 seeds, N+50/+54 provenance, P/P+0→N+14, повторные snapshots/identity, lifecycle, полный message/sender и consumers |
| GAP06 | partial | Оставшиеся H01 prerequisites, исходный producer/strict protocol, поведенческая эквивалентность и независимый внешний аудит |

### Точные границы, которые нельзя считать закрытыми

- Point+26: `0x4149BB92` меняет DWORD `+25..+28`; перенос в +26 условен.
  Pointee не доказан как Point; начальные байты/ранние aliases неизвестны.
- T+F4: `0x414C1ED4` — raw-copy `R+24→U+F4`; `U==T`, producer R+24,
  сохранность ECX/stack и порядок до selector неизвестны.
- G425009BC: store `0x41DA7D1B [Q+8]←W` покрывает байт лишь при нужном Q;
  реальная `FS:[2C]→table[I]→Q`, ненулевой источник и порядок не установлены.
- T+68: helper `0x414AB5D0` не стал доказанным typed consumer; нужны новые
  основания, а не повтор его уже проверенного окна.
- R03: `[0x4226723C]` — snapshot dispatch, не runtime allocator/binding.
  `0x41C5BDA0` сохраняет ESI только при normal-flow/nested-call/stack предпосылках;
  writer `0x41C5E404`, ячейка `0x4252194C`, кандидат `+24←0x3C888889`
  ещё не соединены доказанными lifetime/identity/writer-before-read.
- Entity/weapon: два resolution-вызова `0x414F8567/0x414F8573` независимы;
  проверка первого результата не доказывает валидность второго.
- P-ячейка `0x4223CA04` не равна R03 fallback-ячейке; P+0 writer не является
  автоматически supplier tolerance P+14. Нельзя склеивать наблюдения между вызовами.
- Point/T/H/R/N/P — разные объекты. Static window не равен полному CFG;
  тест собственной модели не доказывает достижимость оригинального пути.

Evidence каждого пункта: [W5 table](../continuation/evidence/wave5/README.md).

## 4. Все 24 области: откуда продолжать

Это очередь **предлагаемых следующих участков**, не новые назначенные задачи.
У каждой строки всё ещё есть незакрытый остаток. Нужны source→transform→consumer,
bytes/hash, предусловия, код при достаточном доказательстве и независимая приёмка.
Начать с строки 3, затем по зависимостям; независимые строки можно делегировать.

Идентификаторы строк совпадают с `domains[].id` в
[карте W5](../continuation/evidence/wave5/scope/coverage.json).
Там же `sources` разрешает архивные пути и их hash/revision; адреса ниже —
навигационные кандидаты, не новые доказанные runtime-привязки.

| № / область | Уже есть | Ближайшая ограниченная работа и оставшаяся граница |
|---|---|---|
| 1 `language_abi` | PE32/i386, RTTI-подобный marker | Привязать один необходимый тип/ABI к телу nl.bin; затем весь транзитивный контур, compiler/standard, а не вывод из одной библиотеки |
| 2 `target_selection` | Локальные фильтры/T+C8, F0 consumer | Одна оставшаяся selector-ветвь `0x41477AF0`; полный порядок ranking/action и T+F4 ещё нужны |
| 3 `point_evaluation` | W5 direct score-prefix/F0 | Non-direct путь `0x4149E7B1` или один tail store после `0x4149E983`; enumeration/best-point и Point+26 остаются открыты |
| 4 `geometry_hit` | Архивные геометрические цепочки | Один capsule caller `0x414F3C90→0x289A6D60`, вход/выход и binding; client-кандидаты `0x10876D60/0x103E6D60` не объединять |
| 5 `chance_thresholds` | T+64 budget, T+68 store/inventory | Новый typed consumer T+68 (`0x4149E315` — store, не consumer); полная geometry/worker/rounding цепочка |
| 6 `damage` | Hitgroup-таблица, локальные math helpers | Один case `0x4136FFA0/0x41370710` до downstream store; weapon/surface providers и весь damage ещё нужны |
| 7 `penetration` | Частичный архивный контракт | Один unresolved provider/финальный выход `0x41371D00`; trace/surface и полный проход препятствий |
| 8 `armor` | Локальная арифметика | Один applicability-выход `0x4136CC20` для `0x41370D00`; исходные operands, коэффициенты и все guards |
| 9 `history` | Архивные H walks/variants | Один вариант `0x413F6DE0/0x413F88F0` с восстановлением и consumer; suitability/matrices/связь с point |
| 10 `settings` | Поля T+54/+60/+64/+68 | Одно неохваченное поле из `0x4149DFE0` до typed consumer; все overrides/modes и порядок |
| 11 `filters` | Локальные eligibility/T+C8/Point+25/+26 | Один новый safety branch selector/point caller с тем же объектом и эффектом; все фильтры и порядок |
| 12 `angle_correction` | Архивная числовая/recoil часть | Одна entry-ветвь `0x414AE770` от `0x4147A2A0`; NaN/Inf/FP порядок, commands и непрямые вызовы |
| 13 `movement_stop` | Один предикат движения | Predecessor/consumer `0x414A5130`, также `0x4148CAE0`; mode/command scheduling. `0x414B2930` — batch dispatcher, не stop-feasibility |
| 14 `modes_dt_hs` | Лимиты и локальные charge contracts | Одна release-ветвь `0x415E0120` с состоянием; затем DT/HS transitions, источники флагов, release scheduling |
| 15 `charge_resets` | Все 11 локальных reset zero stores | Один predecessor блока `0x415DECEE..0x415DED52`; entry predicate, смыслы полей и reset/charge/release порядок |
| 16 `network_state` | N storage/reader, один quarter-decay seed | Другой seed/provenance либо один reader→action; P/P+0→N+14 и весь N+48/+50/+54 lifecycle |
| 17 `messages` | W5 bounded parser | Один enclosing validator/consumer после `0x41512BD0..0x41512BE8`; framing/buffer init, sender, downstream acceptance — без живой сети |
| 18 `shot_result` | Локальные R/Cring контракты | Один store→consumer в `0x414C0A50`; связь command/event/result, причины промаха и server-confirmed hit пока не доказаны |
| 19 `interfaces` | W5 entity/weapon/ESI/dispatch/+24 anchors | Одна доказанная identity/provider граница R03 или entity/weapon; не подставлять готовый runtime binding |
| 20 `ownership_lifecycle` | T move-prefix, N/P snapshots | Один resource-owner transition T/N, включая `P+0→N+14` при `0x414F840B`; lifetime/aliasing/order всего контура |
| 21 `services_queue` | Архивные dispatcher/initializer anchors | Один callback-context lifetime путь `0x414B2930→0x414AC2D0` либо initializer slot 67/155; реальное расписание/ошибки остаются |
| 22 `guards_dispatch` | W5 локальные guard boundaries | Один ещё opaque/exceptional путь или handler `0x4150B6C4..0x4150B6DF`; TLS/FS среда и другие handlers |
| 23 `remaining_consumers` | Известные локальные T/N consumers | Одна новая typed source→reader→branch→action связь T/H/R/Point/N; расширять карту необходимыми зависимостями, не считать список полным |
| 24 `equivalence_audit` | Native/sanitizer/static regression, агентные ревью | Один H01 gate по таблице ниже; полный original behavior protocol и внешний аудит остаются отдельными gates |

### H01: все 12 historical-only записей

[Frontier](../continuation/evidence/wave5/historical/frontier.json) и
[provider review](../continuation/evidence/wave5/review-providers/report.md)
содержат predicates и точные prerequisites. Сохранены **47 dispositions:
33 fresh-static, 2 model-only, 12 historical-only**, отдельно 162 byte checks.

| № | Остаток |
|---|---|
| 5 | Новый pefile 2024.8.26 resource leaf повторён, но original producer/protocol неизвестен; disposition не повышен |
| 6 | LIEF 1.0.0-d05b3499b: нужный build/API, source/resource/hash и отдельный replay |
| 199, 200 | Rizin 0.9.1: exact invocation/base/options и 143 resource / 19 client anchors отдельно |
| 202–206 | VEX: `reload_literal_read`, `height_clamp`, `height_x87_conversion`, `metric_compare`, `strict_threshold`; нужны pyvex/archinfo 10.0.5, точные spans/VA/bytes/lift options |
| 207–209 | Историческая неизменность neverlose/client/engine DLL: нужны original interval hashes/manifests/provenance; сегодняшнее совпадение hash не заменяет старый интервал |

Не устанавливать весь toolchain вслепую. Если нет исходных входов/producer,
зафиксировать конкретный blocker. Агентный reviewer не выполняет внешний аудит;
запрет запуска/эмуляции оригинала не снимается ради H01.

## 5. Немедленный старт и порядок закрытия

1. Получить свежую ветку, проверить SHA/владельцев и восстановить материалы по
   [BOOTSTRAP](BOOTSTRAP.md). Не просить повторно прислать архивы: шесть ZIP уже в Git.
   Не клонировать поверх существующих изменений и не распаковывать поверх HANDOFF.
2. Запустить baseline `make check` с абсолютным HANDOFF и две проверки целостности.
   Прочитать исполняемые архивные скрипты до запуска. Требуемый Git baseline
   `ed9df79c028e63ca56fc191a226f5fe2f221df64` должен быть доступен (углубить shallow).
3. Не ждать карточки `ready`: сейчас их нет. Координатор создаёт **новую** карточку,
   например `NEXT-AGGREGATE`, с фактическим base SHA, owner, write paths и stop rule.
   Старые W5 done не переоткрываются только ради нового исследования.
4. **Первый участок по умолчанию:** один non-direct callback branch
   `0x4149E7B1` из [aggregate contract](../continuation/evidence/wave5/aggregate/contract.json).
   Проследить typed inputs, условия и первый доказуемый эффект, свежие bytes/decodes.
   Counter уже увеличивается до этой ветви: `nullopt` текущего API не есть rollback
   оригинала. Индекс ≥16 и `0x4149EC2D→0x412AF300; INT3` не заменять guessed handler.
   Реализовать только полностью установленный локальный контракт и его тесты.
5. Если этот участок упирается в новый неизвестный provider, сохранить достигнутый
   frontier с адресом/предпосылкой; не повторять broad literal scan. Перейти к одному
   callback-tail store, reset predecessor или enclosing message consumer из таблицы.
6. Параллельно, при доступных инструментах, назначить **независимые** slices:
   language/type evidence, reset/release, message boundary, один новый geometry/damage
   участок. Отдельные worktree/build/write paths; общие status/Makefile — только координатор.
   Не фиксировать число агентов или модель, если пользователь/среда этого не требует.
7. После каждого принятого участка двигаться к следующей зависимости и постепенно
   соединять тела в исходный контур. Не ограничиваться новым планом/переименованием
   unknowns; при достаточных байтах нужен код. Blocked карточки возобновлять только
   с новым конкретным основанием, которое снимает записанный blocker.
8. Закрытие полного GAP — только после выполнения **всех** его обязательств.
   Если дальше остаются только недоступные исходные/runtime/external-audit prerequisites,
   сохранить результаты и точный blocker вместо обещания «всё восстановлено».

## 6. Проверки, режим и публикация

Разрешены статический анализ и запуск собственного офлайн-кода. **Не загружать,
не исполнять, не эмулировать оригинальные DLL; не подключаться к игре и не делать injection.**
Исходные ZIP, восстановленные `workspace/input`, `workspace/work` и архивное evidence
read-only. Новые файлы — в корневую `continuation/`, временные — в игнорируемую `runs/`.
Абсолютные `/tmp/hoplite/...` в старых отчётах — provenance, не переносимые зависимости.

После восстановления, из корня репозитория, при абсолютном `HANDOFF`:

```sh
export PYTHONOPTIMIZE=0 PYTHONDONTWRITEBYTECODE=1
make -C continuation check HANDOFF="$HANDOFF"
python3 -B "$HANDOFF/tools/check_bundle.py"
python3 -B "$HANDOFF/workspace/orchestration/v1/approval_package/check_integrity.py" \
  --root "$HANDOFF/workspace"
git diff --check
```

Последний принятый W5-прогон: **exit 0**, 10 программ, **23 354 assertions в каждом
native/ASan+UBSan режиме**, 13 Python unit tests, 13 новых static verifiers.
Целостность: 960 bundle-файлов, 64 087 input/work записей, шесть ZIP неизменны.
Это историческая подтверждённая база; следующий агент должен выполнить свой прогон.
Не ослаблять тесты ради PASS; сохранять неуспешные прогоны отдельно. `make test`
не заменяет `make check`; missing HANDOFF/baseline — blocker, не skip.

Перед приёмкой нового кода — независимое ревью **фиксированного интегрированного SHA**
в отдельном worktree; после исправлений новый SHA и повтор нужных проверок.
Обновлять STATUS/TASKS/SOURCE_INDEX/DECISIONS и свежие evidence, не старые журналы.
Отчёт: код и границы, источник/hash/адреса/байты, команды и exit codes, проверенный
и опубликованный SHA, результаты ревью и что осталось. Только после этого commit/push
в существующую ветку с явным refspec, обновить PR #1; никаких merge/queue/auto-merge.
Не публиковать распакованные DLL, build, секреты или необработанные приватные логи.
Отсутствие GitHub checks не называть зелёным CI. Полная готовность определяется
[GOAL](GOAL.md), а не успешностью локальной модели или этой таблицей.

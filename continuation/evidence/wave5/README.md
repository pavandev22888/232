# W5: ограниченные результаты и точный остаток

16 исполнителей `gpt-6.1-sol` работали от `3e5fd5f8f1ac3dadd635d19ed8b946ee2567fd3b`
в отдельных worktree. [Назначения](assignment.json), [переносы и проверки](verification.json)
и [независимые ревью](review-assignment.json) сохраняют SHA и области записи.
Интегрированный SHA для ревью: `f2003a7c47d58be6e07c8a9eb2bf7ffe9abfc65a`.
Три ревью завершены без блокирующих замечаний: [код](review-code/acceptance.json),
[evidence/scope](review-evidence/verification.log), [providers/H01](review-providers/acceptance.json).
Приёмка ограничена заявленными контрактами/frontier; это не внешний аудит.

## Результаты

| Задание / evidence | Локальный результат | Что не установлено |
|---|---|---|
| [SCOPE](scope/coverage.json) | Карта 24 областей, 39 source pins; PE32/i386 и RTTI-подобный маркер | Полный call graph, язык/стандарт/toolchain/ABI всего транзитивного контура |
| [POINT26](point26/anchors.json) | DWORD ADD на `+25..+28` может дать перенос в `+26` при явно заданных исходных байтах | Идентичность Point, реальные исходные байты, первый положительный источник и порядок |
| [TF4](tf4/anchors.json) | Кандидат raw-copy `R+24 → U+F4` | `U == T`, положительность/producer R+24 и writer-before-selector |
| [GLOBAL9BC](global9bc/chain.json) | Условия покрытия глобального байта косвенным DWORD store `[Q+8]` | Реальная TLS-привязка Q, ненулевой источник и порядок |
| [AGGREGATE](aggregate/contract.json) | Код direct score-prefix и отдельного условного T+F0 finalizer | Остальные ветви, callback tail, ranking и полный выбор цели |
| [THRESHOLD](threshold/consumer_frontier.json) | Один непосредственный helper-кандидат использует `+50/+54/+58/+19` | Типизированный T+68 consumer, ветвь и действие |
| [BINDINGS](bindings/contract.md) | Закреплена локальная граница entity/weapon resolution | Реальные внешние providers и runtime identity |
| [ESI](esi/preservation.json) | Save/restore для указанного normal-exit пути | Контракты nested calls, сохранность stack slot, exceptional paths |
| [INDIRECT](indirect/dispatch.json) | Закреплён косвенный dispatch через `[0x4226723C]` | Runtime binding, возвращаемый объект и lifetime |
| [FALLBACK24](fallback24/initializer.json) | Кандидат DWORD `+24 ← 0x3C888889` | Идентичность объекта fallback consumer, writer-before-read и lifetime |
| [P-TOLERANCE](p-tolerance/) | В назначенном объёме нет нового byte-level provider сверх W3 | P/P+0, tolerance; отрицательный поиск не доказывает отсутствие writers |
| [GUARDS](guards/) | Закреплены локальные guard/dispatch границы | Полные TLS/FS контракты, непрозрачные переходы и исключения |
| [CHARGE](charge/) | Явный snapshot/reset 11 локальных zero stores: 3 BYTE, 8 DWORD | Entry predicate, расписание и полный автомат |
| [LIFECYCLE](lifecycle/) | Quarter-decay seed соединён с прежним marker tail | Остальные seeds, P/P+0, cross-call identity и полный lifecycle |
| [MESSAGES](messages/) | Parser ограниченных полей и локальная граница вызова consumer | Полный формат, packet framing и sender |
| [HISTORICAL](historical/README.md) | Свежий pefile replay одного resource leaf №5 | Исходный producer, остальные gates, strict old protocol и эквивалентность |

Ссылки на evidence не превращают предпосылки в доказательства. Совпадения
смещений не объединяют Point/T/N/P; статические окна не являются полным CFG.
Новые модули сохраняют C++17 regression baseline, но не устанавливают язык оригинала.
CODE01/GAP01–GAP06 не закрыты; H01 dispositions не повышены по одному leaf.

## Проверки и режим работы

`make -C continuation check HANDOFF=/absolute/path/RAGEBOT_HANDOFF_V10`
проверяет 10 программ: 23 354 assertions отдельно native и ASan/UBSan,
13 Python unit tests и статические gates, включая 13 новых W5 verifiers.
Точные команды, SHA, exit codes и хеши логов — в [verification.json](verification.json).
Инструкция отдельного pefile replay и pinned dependency — в [historical/README.md](historical/README.md).

Исходные DLL не загружаются, не исполняются и не эмулируются. Архивы,
HANDOFF/input/work остаются read-only; build и полные рабочие логи не публикуются.
PR #1 предназначен только для просмотра: без merge, очереди и auto-merge.

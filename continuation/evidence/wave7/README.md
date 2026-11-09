# W7: локальные зависимости и интеграционная проверка

**Ограниченный объём W7 принят. Полный Ragebot не восстановлен.**
База W7 — `24aca6a6fdb8d5f20da528dc658d3d62baa1e133`, интегрированный
кандидат — `562d1c78f2df1343cf9a9e3e3f91a115be2069ac`.
[Verification ledger](verification.json) и [excerpts](verification.log)
содержат команды, exit codes, SHA-256 полных локальных логов и границы проверки.
[Assignment](assignment.json) — исходный снимок; текущие статусы —
[реестр](../../../docs/TASKS.json).

## Реализованный ограниченный объём

| Участок | Source-backed результат | Не восстановлено |
|---|---|---|
| [Ranking](rank/contract.json) | Normal tail `0x4149E9BD..0x4149EC2D`, signed/unsigned comparisons, FP unordered, raw replacement 0x50 bytes | `owner+4→+B0` provider, index handler `0x4149EC2D`, enumeration, полный selector |
| [ABI](abi/contract.json) | Caller `0x414A8D68→0x4149E790`, ECX/EDX observations и обычный эпилог | Source declaration/keyword, RTTI owner, исходный язык/стандарт и полный ABI |
| [Reset](reset/reset_projection_evidence.json) | 25 ordered DWORD stores в caller snapshot 0xC8, сохранение gap bytes; четыре fallthrough stores и прежняя reset-проекция | EAX provider, identity, полный entry predicate/loop и scheduling |
| [History](history/README.md) | No-growth existing-slot resolver `0x413F88F0`, init, отдельные cursor/index, consumer с explicit post-call slot | Growth/allocator, ownership/lifetime, continuity через opaque `0x41B70930`, matrix path |
| [Movement](movement/receipt.json) | Caller `0x4148CC44..0x4148CC8C`: selector, bit0 guard, float bits и AL branch | Полное тело `0x414A5130`, command/global providers и семантика остановки |
| [Damage](damage/README.md) | Только hitgroup 3 в `0x41370710`: observed provider float × 1.25f, return/store anchors | `[vtable+0x30]` provider, trace-result linkage, guards, остальные hitgroups, armor/surface/weapon |

Условия памяти, aliasing, rounding/FTZ/DAZ, исключённые FP effects и отсутствующие
observations определены в локальных API/контрактах. Тест модели не доказывает
runtime reachability, identity или восстановление provider. Local ABI evidence
не устанавливает исходный язык по ECX/EDX или по языку нынешнего C++17-кода.

## Исправления до приёмки

- History раньше записывал masked index вместо decremented cursor. Для capacity
  5 и cursor 4 правильны index 0 и сохранённый cursor 3. [Byte finding](history/coordinator-finding.md),
  [исходный провал](integration/initial-failures/history-cursor/before.log) и
  [свежий повтор](integration/history-cursor-after.log) сохранены раздельно.
  Нельзя предполагать power-of-two capacity. Consumer получает отдельное
  post-provider slot observation, не автоматически прежний resolver result.
- Reset follow-up добавил реальное применение stores к raw snapshot, а не
  только metadata trace; тесты проверяют весь снимок, промежутки и хвост.
- Исполнительские первоначальные провалы сохранены рядом с evidence каждого
  участка; их исправленные PASS не подменяют интеграционный gate.

## Проверки фиксированного кандидата

На `562d1c7` focused native/ASan+UBSan, шесть W7 byte-gates, полный
`make -j2 -C continuation check HANDOFF="$HANDOFF" BUILD=...`,
`check_bundle.py`, `check_integrity.py --root "$HANDOFF/workspace"` и
`git diff --check` завершились с **exit 0**. BUILD и HANDOFF — абсолютные пути
в ledger. Итог: 16 программ на режим, 67 660 инструментированных проверок
на режим плюс три тестовые программы без счётчика; 13 Python unit tests.
Bundle: 960 файлов; input/work: 64 087 записей. Это не процент готовности.

Ранее [rank review](review-rank/report.json) проверил только `1380fca`.
Интеграционные [state](review-state/README.md) и [flow](review-flow/report.json)
ревью приняли фиксированный `562d1c7` без блокеров. Точные commit pins —
в ledger; код, тесты, Makefile и byte-gates после него не менялись.
Это агентное ревью, не внешний аудит или GitHub human approval.

## Неизменные границы и следующий шаг

CODE01/GAP01–GAP06, R02/R03/R04, I01/FULL01 не закрыты. Исходный язык,
runtime providers, полный selector/geometry/damage, identity/lifetime/scheduling,
старый H01 protocol и внешний аудит остаются отдельными требованиями.
Следующая карточка должна иметь новый конкретный source anchor за границей
этих контрактов; повтор уже реализованного slice не считается прогрессом.

Только статический анализ и собственный офлайн-код. Оригинальные DLL не
загружались, не запускались и не эмулировались. PR только для просмотра:
без merge, очереди и auto-merge. Свежий gate итогового публикационного SHA
фиксируется отдельно в PR, без самоссылки будущего metadata-коммита.

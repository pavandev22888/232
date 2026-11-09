# CODE01 — Wave 2: ограниченное read-only integration review

Baseline: `ed9df79c028e63ca56fc191a226f5fe2f221df64`. Все номера строк ниже относятся к
`git show ed9df79c028e63ca56fc191a226f5fe2f221df64:<path>`, а не к одновременно изменяемому worktree.

**Результат:** исходный CODE01 остаётся частичным/открытым. Подтверждённых ошибок
поведения production-функций в этом ограниченном review не выявлено. Найдены
два несогласованных с нормальным возвратом reader значения AL в тестах,
пробелы составного покрытия и ограничение независимости charge-oracle.
Агрегация/target selection не реализовывались и не реконструировались.

## Точный инвентарь завершённости

| Слой | Что действительно есть | Чего это не закрывает |
|---|---|---|
| Восстановленный `src/` | 3 C-файла, **13 публичных тел**: 2 core, 2 netvar, 9 math | Не полный исходный Ragebot |
| Восстановленный `sdk/` | 6 заголовков: типы, частичные раскладки, объявления; inferred signatures в комментариях | Не дополнительные реализации и не доказанный runtime ABI |
| Восстановленный `tests/` | 3 переносимых C-теста и 1 Windows detours-тест | В этом review не запускались |
| `position_records` | 4 свободные функции; конструктор и 2 inline at; N=0x74, 2048 записей, update/item/read/entry gate | Не packet decoder, resolver, полный consumer или исходный index-fault handler |
| `charge_state` | 3 функции: capacity, counter gate, модельная композиция set-only latch | Не полный автомат, reset-lifecycle и не доказанный исходный call order |
| `position_lifecycle` | 2 функции: локальный marker producer и условный actor consumer | Не полный upstream/lifecycle/sender; owner/sink и guards разрешаются вызывающим |

Полный список 13 восстановленных файлов сверён с реальным составом `src/sdk/tests`,
размерами, строками и SHA-256; 10 baseline-файлов сборки/заголовков/исходников/тестов
зафиксированы отдельно в `findings.json`. Это исчерпывающий инвентарь указанных
директорий, не вывод из отсутствующих результатов поиска. Корень восстановленного
кода: `runs/handoff/RAGEBOT_HANDOFF_V10/workspace/work/recovered_nl/recovered_nl/`.

Связь position-модулей доказана на уровне общего N/PositionRead/Vec3.
`charge_state` не вызывается position-модулями. Линковка всех исходников в каждый
тест через Makefile — build integration, а не общий восстановленный runtime-путь.
Текущие README/STATUS честно ограничивают результат; ложного заявления о полном
закрытии CODE01 не найдено. Формула 3170 + 85 + 2601 = **5856** верна.

## Конкретные замечания координатору

1. **W2-CODE01-01 — несогласованные интеграционные fixtures, medium.**
   `tests/test_position_lifecycle.cpp:54–66` получает `source_unavailable`,
   но передаёт `reader_result_al=true`; `:251–259` получает `usable`, но
   передаёт `false`. Для тех же нормальных исходных возвратов это не AL reader:
   `4150BEB6 → 4150C4DF` возвращает ноль, а `4150C4DA SETB AL` даёт
   возрастной результат 0/1. Actor сохраняет результат reader в `414FD841`
   и проверяет BL в `414FD905`. Точные байты проверены в исходном файле,
   SHA-256 `682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e`.
   **Действие:** оставить независимые входы как локальные tests, но явно отличить
   их от связного вызова и добавить согласованные normal-read сценарии.
   Это не ошибка API, который намеренно принимает AL отдельно; неизвестный
   out-of-domain/opaque возврат нельзя получать из host status.

2. **W2-CODE01-02 — составная marker-policy матрица, medium.**
   В `tests/test_position_lifecycle.cpp:218–277` цепь producer→reader покрывает
   положительный marker; произведённые 0/NaN далее не проходят reader.
   Reader policy (`test_position_records.cpp:240–285`) и actor guard
   (`test_position_lifecycle.cpp:76–113`) проверяются отдельно.
   **Действие:** focused chains для 0/NaN с применением/обходом policy, а также
   отрицательного marker при usable/source-unavailable; проверять active,
   status, согласованный AL, переход и сохранность sink. Отдельные ветки уже
   покрыты; неверная реализация ими не доказана.

3. **W2-CODE01-03 — независимость charge oracle, low.**
   `test_charge_state.cpp:39–73` структурно повторяет production-формулы
   `src/charge_state.cpp:32–78`; 144 edge + 2592 grid = **2736** сравнений
   используют этот reference. Есть 8 literal boundaries и 18 raw-byte checks,
   но общее число checks не является независимым доказательством эквивалентности.
   **Действие:** при приёмке изменений charge добавить несколько буквальных
   byte-anchored expectations для wrapping/ветвей, не удаляя grid.

## Воспроизведение и фактические результаты

Из корня репозитория:

```sh
python3 continuation/evidence/wave2/code01/review_baseline.py --run-tests
```

Проверщик извлекает только **собственные baseline C++-файлы** через git show
в `TemporaryDirectory /tmp/code01-baseline-*`. BUILD находится там же, не в
общем `continuation/build`. Запущено:

```sh
make -C /tmp/code01-baseline-_4opp5ka test sanitize BUILD=/tmp/code01-baseline-_4opp5ka/build
```

Результат exit **0**: native и ASan/UBSan — по **3170 + 85 + 2601 = 5856**
assertions. Дополнительный собственный probe собран с ASan/UBSan:
**16 checks passed**, exit **0**; он проверяет согласованные normal-read цепи
source-unavailable/positive marker, usable/zero и expired/zero-or-positive.
Эти синтетические проверки не утверждают поведение работающего оригинала.

Также: **13/13** восстановленных файлов совпали по составу/размерам/строкам/SHA,
**10/10** выбранных byte checks совпали; оба fixture mismatch подтверждены.
Полные выполненные команды, stdout/stderr и точные хеши — в `findings.json`.
Временная сборка удаляется автоматически. Компилятор: g++ 13.3.0.

## Оставшееся и границы

Исходный широкий CODE01 **не закрыт**. Прежнее задание агрегации не выполнено;
оно не повторялось. Полный target selection, T+68 consumer, формат/отправитель,
внешние привязки, полные guards и жизненные циклы не восстановлены. N+68 —
snapshot_type, не T+68. Порядок charge и position в оригинале не установлен.

Ничего не менялось в исходниках, общих документах, старых evidence,
handoff/input/work. Исходные DLL не запускались/не загружались/не эмулировались;
игра, инъекция и live inputs не использовались. Исторический H01, восстановленные
C-тесты и Windows-тест не запускались. Это source review и offline-тесты
собственного кода, не независимый equivalence audit. UI/Preview не затронуты.
Координатору требуется повторная проверка на интегрированном head после чужих
изменений charge_state/position_lifecycle; текущие выводы закреплены за baseline.


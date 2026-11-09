# W3-V: изолированное воспроизведение

Проверен **неизменённый код** `775ac620349bc7ce5c9fd1b56d219135c3c00901`
в отдельном worktree `/tmp/hoplite/worktrees/sikyon-w3-repro`, ветка
`hoplite/sikyon-46eee495--w3-repro`. Git index и build отдельные;
общая только объектная база Git. В этом коммите добавляются лишь эти evidence.
Точные команды, результаты, версии и хеши закреплены в `acceptance.json`;
`inputs.sha256` содержит 71 проверенный файл continuation **до** добавления W3-V.

## Повтор

Нужны GNU make, Git с указанной ниже историей, Python 3.10+, GNU objdump
с декодированием i386 и C++17-компилятор с ASan/UBSan. Проверенный набор:
GCC 13.3.0, Python 3.12.3, GNU make 4.3, GNU binutils 2.42, Linux x86-64.
Python-проверки используют стандартную библиотеку. Windows, загрузчик DLL,
IDA/Ghidra, Wine и эмулятор не требуются. Другие ОС/компиляторы не проверялись.

Из корня указанного worktree реально выполнено:

```sh
make -C continuation check HANDOFF=/tmp/hoplite/workspace/runs/handoff/RAGEBOT_HANDOFF_V10 BUILD=build/w3-repro
sha256sum --check continuation/evidence/wave3/repro/inputs.sha256
```

`check` завершился с кодом 0: пять native и пять ASan/UBSan suites,
**6528 assertions на режим**, 14 команд evidence, включая 8 Python unit tests.
Санитайзеры запускаются штатным Makefile с `ASAN_OPTIONS=detect_leaks=1`
и `UBSAN_OPTIONS=halt_on_error=1`; strict warnings и FP-флаги не менялись.

`HANDOFF` — **отдельно восстановленный полный комплект**, доступный на чтение,
с прежними относительными путями внутри `workspace/`. Здесь он находится вне
worktree, поэтому переносимость относительно корня репозитория проверена
реальным запуском, а не копированием образов в build. Путь можно заменить
другим абсолютным путём к такому же комплекту. Второе физическое расположение
комплекта не создавалось и не проверялось. Путь по умолчанию в этом worktree
отсутствует. Makefile не распаковывает исходные ZIP автоматически.

BUILD изолирует C++-бинарники, **но не все временные результаты evidence**:
старые проверки также используют `continuation/build/evidence_check.json`,
свои временные каталоги и `runs/` внутри данного worktree. Эти штатные проверки
не редактировались; временные окна декодирования удаляются. Нельзя выдавать
BUILD за полную изоляцию двух одновременных запусков в одном worktree.

## Отдельная историческая база

CODE01 reviewer и GAP02 inventory требуют Git-объект
`ed9df79c028e63ca56fc191a226f5fe2f221df64`: они читают закреплённые blobs через
`git show`/`git ls-tree`, а не текущие изменяемые файлы. Проверить доступность:

```sh
git cat-file -e ed9df79c028e63ca56fc191a226f5fe2f221df64^{commit}
```

Архив исходников без `.git` или shallow clone без этого объекта недостаточен
для полного `check`, даже при правильном HANDOFF. Нужна история ветки,
содержащая этот коммит; отсутствие истории здесь искусственно не моделировалось.
Наличие всех необходимых объектов и успешные обращения к ним подтверждены
текущим прогоном. Сам `make check` вызывает CODE01 reviewer **без** `--run-tests`.
Для отдельного свежего запуска именно старого кода дополнительно выполнено:

```sh
mkdir -p runs/w3-repro/temp
TMPDIR="$PWD/runs/w3-repro/temp" ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  python3 -B continuation/evidence/wave2/code01/review_baseline.py \
  --handoff /tmp/hoplite/workspace/runs/handoff/RAGEBOT_HANDOFF_V10 --run-tests
```

Результат — код 0, **5856 assertions на режим** в трёх старых suites и
16 checks штатного sanitizer integration probe. Reviewer временно извлекает
только 10 файлов собственного offline-C++-кода из Git; каталог удалён.
Это не повтор запуска новых модулей и не новая проверка оригинального runtime.

## Неподдержанный gate и предел доказательства

Отрицательная проверка `make -C continuation evidence` с отсутствующим HANDOFF
дала ожидаемый код 2: первая проверка остановилась на `FileNotFoundError`
для `workspace/work/nl_handoff_2026-11-06/nl.bin`. Это **unsupported из-за
отсутствующих внешних входов**, не PASS без DLL и не дефект полноценного прогона.
Точная команда и хеш лога сохранены в `acceptance.json`; сообщение сейчас
представлено Python traceback, специального preflight в Makefile нет.

H01 остаётся ограниченным: 162 byte assertions повторены; среди остальных
47 пунктов — 33 freshly rechecked, 2 model-only и 12 historical-only.
Пересекающиеся проверки не складываются в новые независимые доказательства.
`all_209_repeated` и `strict_old_protocol_repeated` — **false**.
Полный старый VEX/IR-протокол и исходный интервал аудита не восстановлены.
Не доказаны достижимость, runtime-binding, полная эквивалентность и поведение
исходного MXCSR; GAP06 полностью не закрыт.

До/после всех запусков совпали набор, SHA-256, размеры и mtime 65550 файлов
HANDOFF (1300192247 байт). Это целостность **данного** интервала, не повтор
исторического аудита. Все 71 tracked continuation-файл также остались прежними.
DLL/исходные образы не загружались, не исполнялись и не эмулировались;
игровых подключений/injection не было. Исполнялся только собственный offline-C++
и прочитанные Python-проверки; objdump декодировал образы как данные.

Шесть исходных ZIP **уже находятся в upstream main** на закреплённом локальном
`origin/main = d7e41026d179e20fa9e93c1720645181f1686b69`; их Git blob IDs
совпадают с проверенной базой. Новые архивы/DLL в этой работе не добавлялись,
старые не удалялись и не изменялись. Никаких новых tools/CI, production-правок,
push, merge или cherry-pick не выполнялось.

Полные логи и before/after-манифесты оставлены только в ignored
`/tmp/hoplite/worktrees/sikyon-w3-repro/runs/w3-repro/`; их SHA-256 закреплены
в `acceptance.json`. Результат относится к указанным SHA, а не к будущей
интеграции: свежая независимая приёмка W4 всё ещё нужна.

# Восстановление из Git и воспроизводимые проверки

Все команды ниже — из корня репозитория, в shell с `set -eu`. Секреты и
вложения из чата не требуются; для приватного репозитория нужен обычный
разрешённый доступ GitHub. Не передавайте токены в командах/отчётах.

## 1. Получить правильную ветку и историю

Для **нового** клона (существующий рабочий каталог не перезаписывать):

```sh
git clone --branch hoplite/tanagra-adb72df5 https://github.com/pavandev22888/232.git 232
cd 232
set -eu
export PYTHONOPTIMIZE=0 PYTHONDONTWRITEBYTECODE=1
git status --short --branch
git rev-parse HEAD
```

PR #1 слит 2026-11-09: `main` содержит W1–W7 squash-коммитом `0fe371b`.
Ветка выше содержит продолжение W8 и исходную историю для baseline-проверок.
В существующей среде оставайтесь на назначенной ветке. Baseline-review вызывает
`git show` для `ed9df79c028e63ca56fc191a226f5fe2f221df64` — одного shallow HEAD мало:

```sh
if ! git cat-file -e ed9df79c028e63ca56fc191a226f5fe2f221df64^{commit} 2>/dev/null; then
  if test "$(git rev-parse --is-shallow-repository)" = true; then
    git fetch --deepen=100 origin hoplite/tanagra-adb72df5
  else
    printf '%s\n' 'Baseline отсутствует в полной истории: проверьте репозиторий/ветку.' >&2
    exit 1
  fi
fi
git cat-file -e ed9df79c028e63ca56fc191a226f5fe2f221df64^{commit}
```

Отсутствующий baseline проверяется внутри `if`, чтобы `set -e` не оборвал shell
до углубления shallow-истории. Если 100 коммитов недостаточно, углубите ещё и
повторите проверку перед продолжением. В полной истории недоступный baseline —
повод проверить репозиторий/ветку, а не отключать gate.

## 2. Зависимости

Нужны Python **3.10+** со стандартной библиотекой, Git, GNU make, GCC/G++
с C++17 и ASan/UBSan, GNU binutils (`objdump` с поддержкой i386).
Нынешний `make check` проверяет C++17-модули; это не устанавливает язык оригинала.
GCC также используется архивными собственными C-helpers; обязательный C17-порт отменён.
В проверенной Linux-среде: Python 3.12.3, GCC/G++ 13.3.0, make 4.3,
objdump 2.42. Это наблюдавшиеся версии, не требование побайтово идентичной среды.

```sh
python3 --version
git --version
make --version
gcc --version
g++ --version
objdump --version
objdump -i
```

Capstone, pip-пакеты, Windows, IDA/Ghidra, Docker и CMake для этого протокола
не нужны. `python -O` запрещён: архивные скрипты используют `assert` как gate.
Не включайте `-ffast-math` или FP contraction и не отключайте sanitizers.

## 3. Собрать исходный комплект

Шесть `RAGEBOT_V10_часть_XX_из_06.zip` — ZIP-обёртки с payload-фрагментами.
**Не объединять ZIP-файлы через `cat`**. Используется уже поставленный assembler:

```sh
mkdir -p runs/assembly runs/handoff
python3 - <<'PY'
from pathlib import Path
from zipfile import ZipFile
import hashlib
pins = {
    'assemble_handoff.py': 'd51620df0737e05d8c537874f72755b4a5408ceb97d2c318f5fabf590ca3a06b',
    'PARTS_MANIFEST.json': '8144413f466287189e8207fab6ca137661a984d008f660f6b7ac442fe44c67b4',
}
with ZipFile('RAGEBOT_V10_часть_01_из_06.zip') as z:
    for name, expected in pins.items():
        data = z.read(name)
        assert hashlib.sha256(data).hexdigest() == expected, name
        (Path('runs/assembly') / name).write_bytes(data)
PY
cat runs/assembly/assemble_handoff.py
python3 runs/assembly/assemble_handoff.py --parts-dir "$PWD" \
  --output "$PWD/runs/assembly/RAGEBOT_HANDOFF_V10.zip"
```

Assembler проверяет номера, размеры, SHA256 каждого payload и всего архива.
Ожидаемый SHA256 полного ZIP:
`b5e3336edaa99b6f437f22368ab230107b7b781d9dfd54c83c6e00d9cfb8018f`.
Нет одной части — восстановление неполно; отличается хеш — остановиться.
Существующий правильный результат переиспользуется; отличающийся файл или
`.partial` нельзя молча удалять/перезаписывать.

## 4. Восстановить input/work

Для первого запуска используйте пустой путь назначения:

```sh
test ! -e runs/handoff/RAGEBOT_HANDOFF_V10
python3 -m zipfile -e runs/assembly/RAGEBOT_HANDOFF_V10.zip runs/handoff
export HANDOFF="$PWD/runs/handoff/RAGEBOT_HANDOFF_V10"
cat "$HANDOFF/tools/check_bundle.py" "$HANDOFF/tools/bootstrap.py"
cat "$HANDOFF/workspace/orchestration/v1/approval_package/check_integrity.py"
python3 -B "$HANDOFF/tools/check_bundle.py"
python3 -B "$HANDOFF/tools/bootstrap.py"
```

Если HANDOFF уже существует, **не распаковывайте поверх него**: задайте его
абсолютный путь, прочитайте скрипты и выполните две проверки. Bootstrap проверяет
комплект, безопасно восстанавливает `workspace/work` включая вложенный
`functions.zip`, учитывает кодировку имён и проверяет хеши. Существующий `work`
он не перезаписывает. Остаток `.work_restore` требует выяснения причины сбоя.

Ожидаемая целостность: **960 файлов bundle** и **64 087 записей input/work**.
Это целостность материалов, не доказательство поведения. Handoff имеет собственную
папку `continuation/`: новый код репозитория находится **не в ней**.

## 5. Проверить текущий код

```sh
make -C continuation check HANDOFF="$HANDOFF"
python3 -B "$HANDOFF/tools/check_bundle.py"
python3 -B "$HANDOFF/workspace/orchestration/v1/approval_package/check_integrity.py" \
  --root "$HANDOFF/workspace"
git diff --check
git status --short
```

`check` включает native, ASan/UBSan и static evidence. Исторический W5-набор:
23 354 assertions **на каждый режим**, десять программ и 13 Python-тестов;
13 новых static verifiers включены в `make check`. W6 сохранена в
`continuation/evidence/wave6/verification.json`; текущий W7 gate — в
`continuation/evidence/wave7/verification.json`: 16 программ на режим,
67 660 counted checks плюс три программы без счётчика, 13 Python-тестов
и шесть новых byte-gates. Исторические PASS не подтверждают новый HEAD. Историческая база W3–W4
имела 19 902 assertions, семь программ и восемь Python-тестов.
H01: 162 byte checks плюс 33 fresh-static / 2 model-only / 12 historical-only;
полный старый протокол не закрыт. Отдельный W5 pefile leaf №5 воспроизводится
по [инструкции](../continuation/evidence/wave5/historical/README.md) с одной
hash-pinned зависимостью; он не повышает исторические dispositions. `make test` без evidence не заменяет `make check`.

Для первичного повтора архивных V10–V6 (дополнительно к текущему коду):

```sh
cat "$HANDOFF/tools/replay.py"
python3 -B "$HANDOFF/tools/replay.py" --versions v10 v9 v8 v7 v6
```

Обёртка копирует старые verifiers в отдельный `HANDOFF/runs/replay_*` и не
изменяет архивные evidence. Не запускайте старые `verify_v*.py` напрямую:
часть пишет файлы рядом с собой. Оригинальные DLL не исполняются; допустимы
статические проверки и сборка собственных/ранее рассмотренных чистых helpers.

## Изоляция и диагностика

- Передавайте абсолютный `HANDOFF` каждому worktree. Отдельный `BUILD` изолирует
  C++-бинарники, но не все evidence outputs; параллельные прогоны требуют worktree.
- Записывайте SHA, команды, exit codes и версии. Провал и повтор храните отдельно.
- Missing baseline/HANDOFF/objdump/sanitizer — конкретный блокер, не PASS.
- Не коммитьте `runs/`, build и распакованные материалы. Корневой `.gitignore`
  делает это правило переносимым, без локальных настроек прежней среды.
- Свежий результат проверки этого runbook: [handoff verification](../continuation/evidence/handoff/verification.json).

# W5-HISTORICAL: один ресурсный leaf H01

На базе `3e5fd5f8f1ac3dadd635d19ed8b946ee2567fd3b` выполнен **один новый
статический leaf именованной библиотеки**: pefile 2024.8.26 заново разобрал
ресурс `BINARY/101` в закреплённом `neverlose.dll`. Это повтор предиката
архивного H01 №5, **не запуск исходного производителя H01 и не полный старый
протокол**. Никакие архивные поля `pass` не использованы как свежий результат.

Назначение и единственный владелец сверены с главным `docs/TASKS.json`
на `dada7f0` и `continuation/evidence/wave5/assignment.json`; точные SHA
реестра/манифеста закреплены в `frontier.json`. Общие status/docs/Makefile
не менялись. Исходные 47 dispositions остаются неизменными.

## Проверенный результат и байтовые опоры

`leaf.json` — просмотренный stdout нового запуска, exit **0**:

- Тип `BINARY`, ID **101**, язык **1049**; raw offset **0x629B10**,
  размер **0x3501000** (55 578 624 байта), RVA **0x630110**.
- Запись данных по file offset `0x629AC0`:
  `10016300001050030000000000000000` (RVA, размер, нулевые codepage/reserved).
- ID-запись по `0x629A30`: `6500000060000080`;
  language-запись по `0x629A70`: `19040000c0000000`.
- Полные байты ресурса равны закреплённому `nl.bin`, SHA-256
  `682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e`.
- SHA-256 `neverlose.dll`:
  `c1ce8c6d6f1172114d3cff6efe147903292be5eaa6e1c2e28495440fa8f7982c`.

Использована та же версия pefile, что записана в архивных `versions` и отчёте.
Wheel SHA-256 `76f8b485dcd3b1bb8166f1128d395fa3d87af26360c2358fb75b80019b957c6f`
получен из [PyPI JSON](https://pypi.org/pypi/pefile/2024.8.26/json) и проверен
до установки. Hash самого `pefile.py` проверяется до импорта;
`PE(data=..., fast_load=True)` и `parse_data_directories` прочитаны в этой
версии. Парсится только каталог ресурсов. DLL передаётся как массив байтов:
нет системного загрузчика, отображения исполняемого образа, исполнения,
эмуляции, instruction trace, игры или injection.

## Воспроизведение

Из корня выделенного worktree; нужен Python 3.10+ и **один** небольшой
pure-Python пакет. Установка и весь stdout находятся в собственном ignored
`runs/`, никогда не в shared HANDOFF:

```sh
set -eu
HANDOFF=/tmp/hoplite/workspace/runs/handoff/RAGEBOT_HANDOFF_V10
RUN="$PWD/runs/w5-historical"
mkdir -p "$RUN/deps"
python3 -m pip install --only-binary=:all: --require-hashes --no-deps --no-compile \
  --target "$RUN/deps" -r continuation/evidence/wave5/historical/requirements.txt
PYTHONPATH="$RUN/deps" python3 -B \
  continuation/evidence/wave5/historical/replay_pefile_leaf.py \
  --handoff "$HANDOFF" > "$RUN/leaf.stdout.json" 2> "$RUN/leaf.stderr.log"
make -C continuation check HANDOFF="$HANDOFF" BUILD=build/w5-historical
```

Фактически wheel сначала загружен и проверен по PyPI JSON, затем установлен
локально. Hash-pinned установка из `requirements.txt` дополнительно выполнена
с `--no-index --find-links` этого собственного downloads-каталога, exit **0**.
Пакеты LIEF/pyvex/archinfo, Rizin и широкий toolchain не устанавливались.
`verify_v7.py` прочитан и закреплён hash, но не исполнялся: он пишет рядом с
собой и запускает дополнительные helpers. `tools/replay.py`, пишущий в
`HANDOFF/runs/`, также не запускался. Обёрток над старыми verifiers, пишущими файлы, нет;
новая программа только читает входы и пишет stdout в каталог исполнителя.

## Все 12 исходных historical-only пунктов

Полные предикаты, версии, наблюдаемые prerequisites, ссылки на конкретные
JSON pointers и SHA-256 источников — в `frontier.json`.

| H01 № | Старый gate | Результат W5 / конкретный frontier |
|---|---|---|
| 5 | pefile: BINARY/101, offset/size/language | Новый library leaf PASS; точный старый producer не запущен |
| 6 | LIEF 1.0.0-d05b3499b: независимо тот же ресурс/хеш | LIEF отсутствует; нужен отдельный именованный parser replay, pefile его не заменяет |
| 199 | Rizin 0.9.1: 143 resource address/bytes/length | Rizin отсутствует; нужны точная invocation/options; pinned anchors доступны |
| 200 | Rizin 0.9.1: 19 client address/bytes/length | То же, но свой вход client.dll; GNU не заменяет Rizin |
| 202 | VEX reload_literal_read: 45 байт / 14 инструкций | Нет pyvex/archinfo 10.0.5; нужны точные start VA/bytes/lift options |
| 203 | VEX height_clamp: 24 / 4 | Те же prerequisites, свой точный span не задан в metrics |
| 204 | VEX height_x87_conversion: 28 / 5 | Те же prerequisites; IR не является наблюдением x87 rounding |
| 205 | VEX metric_compare: 21 / 4 | Те же prerequisites, свой точный span не задан в metrics |
| 206 | VEX strict_threshold: 29 / 6 | Те же prerequisites, свой точный span не задан в metrics |
| 207 | neverlose.dll unchanged после исходного аудита | Нужны исходные границы/manifest/provenance интервала; нынешний hash недостаточен |
| 208 | client.dll unchanged после исходного аудита | То же для client.dll |
| 209 | engine.dll unchanged после исходного аудита | То же для engine.dll |

У VEX `metrics#/vex` есть labels и counts, но нет start VA, byte string или
lift options. Это утверждение о цитируемых записях, не вывод об отсутствии
возможного источника во всём комплекте. Блоки по названиям не угадывались.

H01 ledger явно записывает `script_present=False`; в трёх конкретных путях,
перечисленных в `frontier.json`, `verify_history_metrics_current.py` сейчас
отсутствует. Это ограниченное наблюдение, не доказательство отсутствия файла
во всех материалах. Архивная команда из отчёта без producer-кода не повторена.

## Проверки и предел

Полный локальный `make check` — exit **0**: семь программ, **19 902 assertions
в native и отдельно ASan/UBSan**, восемь существующих Python-тестов и evidence.
Это сохранение регрессионной базы, **не ещё один новый historical gate**.
Своего C++-кода здесь нет. Синтаксис новой Python-программы и согласованность
12-entry inventory / output / digest проверены отдельно.

В `acceptance.json` закреплены точные команды, exit codes и хеши логов и
before/after-манифестов **нынешнего** интервала. Совпали набор, SHA-256,
размеры и mtime всех 65 038 файлов данного HANDOFF (1 223 394 840 байт).
Это не воспроизводит интервалы №207–209 и не утверждает полноту старого bundle.
Шесть tracked ZIP также не изменены. Никакие DLL/сборки/пакеты не добавлены в Git.

**Proven:** ресурсный предикат №5 заново подтверждён указанной версией pefile
на закреплённых байтах. **Conditional:** это соответствует декларации старого
leaf, но точный исходный код/режим H01 неизвестен. **Unresolved:** остальные
11 именованных gates, весь строгий протокол, достижимость/поведенческая
эквивалентность и независимый внешний аудит. Агентное ревью последнего не даёт.
`strict_old_protocol_repeated`, `all_209_repeated`, `external_independent_audit`
и `full_gap06_closed` остаются **false**. Ни model-only, ни история не повышены
до original-equivalence. На одном replay остановлено.

Координатору остаются полный check интегрированного SHA и независимое fixed-SHA
ревью перед приёмкой. Внешний аудит — отдельное незакрытое обязательство.

# GAP05 wave2 — optional N+50 output

Минимальное расширение существующего consumer: `0x414FDAB4..0x414FDB4C`.
Upstream seed `0x414F85xx..0x414F868B` **не расширен**. Существующие producer-tail,
восстановление указателя N перед записью `N+48`, ordered-nonzero/NaN-ветвление,
формула и разные `ActorGuardOutcome` сохранены.

## Подтверждённый порядок

`EBP` — N, `[esp+3C]` — внешний position sink, `[esp+40]` — marker output,
`[esp+44]` — новый optional output. Это разные роли, не поля одного actor.

1. Только после успешной записи позиции ненулевой `[esp+44]` инициализируется:
   при выключенном выбранном cfg-байте `+274/+275` — `1.0f`; иначе
   `elapsed = now - float(N+14)`, `scaled = 0x1.555556p+0f * elapsed`.
   Сначала маска `elapsed < 0` выбирает `+0.0f` вместо scaled, затем маска
   `0.75f < elapsed` выбирает `1.0f`. Это не `std::clamp`: NaN остаётся NaN,
   а `-0.0f` не заменяется положительным нулём.
2. Затем optional marker output получает N+48.
3. В `0x414FDB48` выполняется `MAXSS(N+50, current_output)`. Второй операнд
   выигрывает при равенстве, включая разные знаки нуля, и при unordered.
   Поэтому `std::max` с обратным порядком или `std::fmax` здесь некорректны.

Нормальные copy-marker/inactive/marker-gate выходы входят в пункт 2, обходя
инициализацию: MAXSS использует прежний output вызывающего кода. Clear-marker
и missing-record выходы обходят оба N+50 store.
Искусственный `trace_observation_missing` не является исходным нормальным
выходом: сохраняется старое поведение marker output, новые N+50 stores
пропускаются. В этих трёх случаях отдельный новый output не меняется, но при
совпадении двух внешних scalar-указателей его меняет marker store.

## Входы, alias-контракт и открытые границы

`ActorSide50Output` принимает внешний float*, разрешённый выбранный cfg-байт
и now из объекта за `0x4223CA04`. Поля `N+14` и `N+50` остаются непрозрачными:
чтение — через `memcpy` и проверенные смещения, без изменения position_records.
Реализация `0x41C784B0` не дублируется; результаты helper по-прежнему явные.

Новый output — корректный инициализированный внешний float, не перекрывающий
N, оба position sink или объекты входов. Он **может совпадать с внешним
marker_output**: тест доказывает порядок initial store → marker copy → MAXSS.
Старый self-alias `marker_output == &N.validity_marker` также сохранён.
Частичные перекрытия и прочие aliases не входят в контракт.

Остаются открытыми upstream seed, происхождение N+14/N+50 вне известных срезов,
прочие writers/resets, runtime binding/reachability, TLS/FS, opaque dispatch,
index/HLT и полный sender/formats. Нет заявления об эквивалентности MXCSR,
исключений, signaling NaN и FTZ/DAZ.

## Свежая статическая проверка

`optional_output.json` фиксирует SHA-256 и длину образа, полные байты и SHA-256
четырёх окон, **87 инструкций / 348 байт**, три float-константы и границы модели.
Проверщик повторно читает образ и декодирует все окна GNU objdump. `--handoff`
только читается; временные raw-snippets создаются и удаляются исключительно
в `runs/wave2/gap05`, без импорта handoff-модулей и создания там pycache.
Образ не загружается как executable, не исполняется и не эмулируется.

Из корня репозитория:

```sh
python3 continuation/evidence/wave2/gap05/verify_optional_output.py \
  --handoff runs/handoff/RAGEBOT_HANDOFF_V10
g++ -std=c++17 -O2 -g -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow \
  -fno-fast-math -ffp-contract=off -Icontinuation/include \
  continuation/src/position_records.cpp continuation/src/position_lifecycle.cpp \
  continuation/tests/test_position_lifecycle.cpp \
  -o runs/wave2/gap05/test_position_lifecycle
runs/wave2/gap05/test_position_lifecycle
```

Для ASan/UBSan заменить `-O2` на `-O1`, добавить
`-fno-omit-frame-pointer -fsanitize=address,undefined`, сменить имя бинарника на
`test_position_lifecycle_sanitize`; запускать с
`ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1`.
Логи и свежий decode находятся в `runs/wave2/gap05`.

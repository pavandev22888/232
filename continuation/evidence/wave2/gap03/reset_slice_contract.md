# GAP03 wave 2: явный условный reset-срез

`apply_charge_reset_slice(ChargeState&, SmallGateInput&)` добавляет один
сброс уже представленных полей. Это не реализация всего исходного reset-блока:
модель проецирует три его записи на явно переданный снимок тех же глобальных
значений.

| Поле модели | Запись нуля | Общий адрес | Чтение в small gate |
|---|---|---|---|
| `mode_latched` | `0x415DECF5`, BYTE | `0x4223F42C` | `0x414F30C9` |
| `guard_c` | `0x415DED2E`, BYTE | `0x425009D4` | `0x414F30E3` |
| `previous_counter` | `0x415DED3F`, DWORD | `0x425056E4` | `0x414F30DB` |

Последний адрес сравнивается с `current_counter - 1` после DWORD `DEC`
в `0x414F30D8`. Сброс не меняет `current_counter`; существующее оборачивание
счётчика сохраняется. Запись единицы в `0x414F12D5` использует тот же адрес
latch. Нулевой raw AL по-прежнему не сбрасывает latch в charge-gate:
новый reset вызывается только явно, не из `compose_charge_model_step`.

`capacity`, кэшированный модельный `small_gate_result`, `guard_a`, `enabled_d`
и `current_counter` эта операция оставляет без изменений. Helper не вызывается
автоматически. Восемь остальных записей оригинального блока остаются вне
проекции; они включены в байтовый контекст, а не заменены заглушками.

## Условие входа и незакрытая граница

Вызывающий код отдельно предполагает исполнение исходного reset-блока и
идентичность переданного gate-снимка указанным глобальным адресам. Предикат
входа, вызывающие функции, достижимость, полный срок жизни latch и порядок
reset → charge-gate → small-gate не доказаны. Последовательности в тестах —
синтетическое расписание модели, не восстановленный call graph. Другие
прямые, косвенные или широкие писатели не исключены. Внешние поставщики,
TLS/FS/opaque guards и release-dispatcher этой правкой не меняются.

Сохранённый `historical_reset_block_walk.json` объявляет аналитический диапазон
`[0x415DECEE, 0x415DED50]`, но его последняя инструкция начинается
в `0x415DED49` и занимает 10 байтов, до `0x415DED52` включительно. Свежий
objdump проверяет полную инструкцию и выход `0x415DED53`; ни одна из этих
границ не объявляется началом/концом runtime-функции. Сохранённый release-walk
v2 начинается только в `0x415DFFA0` и не подтверждает вход в этот reset-блок.

## Воспроизведение

Из корня репозитория, без общего Makefile и без записи в handoff:

```sh
mkdir -p runs/wave2/gap03
g++ -std=c++17 -O2 -g -fno-fast-math -ffp-contract=off \
  -Wall -Wextra -Wpedantic -Werror -Icontinuation/include \
  continuation/src/charge_state.cpp continuation/tests/test_charge_state.cpp \
  -o runs/wave2/gap03/test_charge_state
runs/wave2/gap03/test_charge_state
g++ -std=c++17 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined \
  -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic -Werror \
  -Icontinuation/include continuation/src/charge_state.cpp \
  continuation/tests/test_charge_state.cpp \
  -o runs/wave2/gap03/test_charge_state_sanitize
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  runs/wave2/gap03/test_charge_state_sanitize
python3 continuation/evidence/wave2/gap03/verify_reset_evidence.py \
  --handoff /tmp/hoplite/workspace/runs/handoff/RAGEBOT_HANDOFF_V10
python3 continuation/evidence/gap03/verify_charge_evidence.py \
  --handoff /tmp/hoplite/workspace/runs/handoff/RAGEBOT_HANDOFF_V10
```

Новый verifier сверяет неизменные SHA-256, сохранённые инструкции, полные
байтовые диапазоны и свежий objdump для reset, set-only latch и small gate.
Отдельно сверяются абсолютные операнды трёх пар reset/consumer и set/reset
latch. Исходный образ используется только для чтения байтов и статического
декодирования; он не загружается, не исполняется и не эмулируется.

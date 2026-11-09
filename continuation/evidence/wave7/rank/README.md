# W7: локальный ranking-tail callback

`rank_callback_point` продолжает принятую W6 boundary `0x4149E9BD` до
нормального возврата. [Контракт](contract.json) фиксирует порядок ranking,
приоритеты, DWORD wrap и FP unordered; [anchors](anchors.json) содержат
непрерывное окно и константы. Исходные W6 API не меняют поведения.

Ненулевой C0 сохраняется; замена копирует все 0x50 байт. `owner+4→+B0`
передаётся как optional observation: отсутствие останавливает только путь,
который действительно читает его. Это не доказанный runtime provider.

```sh
python3 -B continuation/evidence/wave7/rank/verify_source_bytes.py --handoff "$HANDOFF"
```

Свежие узкие проверки: [receipt](verification.json). Полная интеграционная
приёмка и независимое ревью записываются на уровне W7; этот receipt их
не заменяет. Enumeration, selector, settings binding, index handler,
исходный язык и полный ABI остаются открытыми.

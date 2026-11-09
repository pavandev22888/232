# W8: ограниченная реконструкция и аудит истории

**Полная цель не достигнута.** W8 добавляет условные офлайн-тела и байтовые
проверки, а не исполняемый оригинальный Ragebot. C++17 — язык реконструкции;
исходный язык, полный ABI, runtime bindings и внешняя эквивалентность не установлены.

Кандидат кода: `bc89d9d851d710208659fc702f1094a317023ab4`. Фактический статус
интеграционного gate и независимых ревью — в [verification.json](verification.json).
Исторические исполнительские PASS не заменяют приёмку интегрированного SHA.

## Добавленный локальный результат

| Участок | Реализация / доказательство | Точная незакрытая граница |
|---|---|---|
| Callback | [Контекст и raw32-загрузки](aggregate/contract.json), цепочка index-handler | Реальные settings/owner providers, `0x4200AC82`, runtime binding `[0x421B8F14]`, исключительный unwind |
| Damage | [Восемь switch-целей и guarded hitgroup-1](damage/contract.json), упорядоченные float stores | TLS initializer `0x41370B4B`, vtable providers, trace/armor/surface/weapon связи |
| Movement | [Предикат `0x414A5130`](movement/verification.json) и сохранённый caller | Командные/global providers, непройденные helper/guard пути и полная функция |
| Message | [Условный receiver-prefix и slot-byte gate](message/byte_anchors.json) | Идентичность runtime receiver, полный формат и sender |
| Geometry | [Post-call result consumer](geometry/contract.md), SSE-порог и записи | Producer/trace/hitbox bindings, достижимость и исходная идентичность объектов |
| Provider ABI | [ESI через вложенный fill-helper](provider/contract.json) | Наличие локального writer не доказывает lifetime, writer-before-reader или runtime provider |
| Reset | [Шесть raw 0xC8-записей и fallthrough](reset/README.md) | Входной predicate, EAX/helper providers, значения полей и полное DT/HS scheduling |
| History | [Growth `0x413F3890`](history/contract.json): request, aligned header, copy/zero, release-request, publication и consumer | Аллокаторы/release, null-slot `0x41DA7BAC`, lifetime и непрерывность через opaque calls |
| Angle | [AX guards и `0x414AF113..0x414AF1D6`](angle/source_anchors.json), ordered wrap/clamp и две записи | `0x42026D1B` / `0x42031040`, command/settings/recoil providers, EDI identity и reachability |
| Shot | [Signed остаток по 150, stride 0x98 и post-call consumer](shot/contract.json) | `0x413F8AB0` / `0x414BD940`, промежуточный CFG, R/0xF1A0 и hit/miss/server meaning |
| Queue | [Raw capture → task → wrapper → worker claim](queue/anchors.json) | Enqueue `[EDX+0x50]`, wait `[0x425057F0]`, returned-handle identity, ownership и завершение callbacks до повторного использования стека |

Ни observations, ни raw32-адреса не являются живыми host-указателями или
подставленными providers. Очередь — последовательная проекция наблюдений,
не реализация синхронизации, scheduler или доказательство безопасного lifetime.

## Замечание ревью и регрессия

Независимый flow-review обнаружил P2: non-volatile `MULSS` мог быть переиспользован
unity/LTO-оптимизацией после смены MXCSR. Исправление `dc18d0b` сохраняет
operand order и добавляет `volatile`/memory barrier. Постоянный `make mxcsr`
включён в `make check`: одинаковые inputs при nearest/upward должны дать
`0x3FA00001` / `0x3FA00002`. [Первоначальный провал и исправленный прогон](integration/damage-mxcsr/)
сохранены раздельно; остальные native/sanitizer и байтовые gates не ослаблены.

## Что означает аудит «всей истории»

[Аудит](audit/history.json) закрепляет доступный срез: **93 коммита, 14 refs**,
metadata/changed paths, 24 области покрытия, архивные source receipts и
[сохранённое состояние публикации](audit/publication.json). Это не постоянно
растущий счётчик всех refs после работы агентов. Доступны лишь **два PushEvent**;
полный серверный журнал pushes, удалённые refs и недоступные коммиты неизвестны.
PR #1 слит; его отсутствие checks/reviews не является зелёным CI.
Старая линия Git сохранена в ancestry для baseline `ed9df79` после squash в main.

## Продолжение

Все CODE01/GAP01–GAP06 сохраняют open/partial-статусы. После сверки владельцев
можно назначить один новый source-backed frontier из таблицы — не повторять
завершённые slices или широкие отрицательные поиски. Полные first-positive
sources, typed T+68, N+14/P/P+0, H01 protocol и внешний аудит также остаются открыты.
Оригинальные DLL не загружались, не исполнялись и не эмулировались.

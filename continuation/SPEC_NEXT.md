# Спецификация bounded offline-композиции

> Контракт принятой C++17-базы W3–W4, не новая задача. Конечная цель теперь
> [читаемый C17](../docs/GOAL.md); перенос пока не реализован и не отменяет
> семантических ограничений ниже.

## Общие требования

1. Переиспользовать `update_resolved_scalar` и существующий lifecycle-модуль,
   не копировать их формулы. Сохранить старые публичные контракты и тесты.
2. Новые функции — C++17 offline API, без поиска объектов/модулей, runtime hooks,
   target selection, packet sender или неявных глобальных чтений.
3. Поля N читать/писать по уже проверенным offsets через `memcpy` и static_assert.
   Непрозрачные поля не переименовывать без доказанной семантики.
4. Сохранять порядок float-операций, quiet NaN, signed zero, MAXSS second-operand
   selection, early unordered/equality return. Без fast-math/FP contraction.
   Исходный MXCSR, sNaN traps/flags, FTZ/DAZ вне принятой области.
5. Внешние наблюдения задавать для каждого вызова отдельно. Общий адрес чтения
   не доказывает одинаковое значение в разные моменты.
6. Отсутствующие наблюдения возвращают явный unresolved outcome. API композиции
   вычисляет на локальной копии и коммитит результат только после разрешения
   всех нужных шагов: при unresolved исходная запись не меняется. Это удобство
   офлайн API, **не** доказанное поведение оригинала при ошибке/отсутствии данных.
7. Переданные observations — значения/snapshot, не alias на изменяемую N.
   Не скрывать предпосылки реального вызова и полную достижимость.

## W3-M: N+50/+54 → N+48

- Входы: N, derived-branch, seed, mode byte, side54 target selector,
  два независимых optional fallback-step наблюдения.
- Direct branch и zero/unordered seed повторяют существующий producer без
  ненужных helper calls/fallback требований.
- Ordered nonzero seed: текущее значение helper берётся из прежних N+50/+54,
  target — соответствующие seeds, **не** наоборот.
- Первый вызов: divisor .25, lower 0, upper 1, tolerance binary32 .01,
  requested_step -1. Второй: divisor binary32 .025, те же lower/upper/tolerance.
- Вызовы по порядку +50, затем +54. Equality/unordered helper может завершиться
  без fallback; отсутствие действительно нужного fallback — unresolved.
- После успешного разрешения передать результаты старому producer, сохранить
  старую формулу и затронуть только его поля. Тестировать rollback на втором
  unresolved после успешного первого шага и сохранность остальных байтов N.
- Источники: wave2/gap04 call-sites `414F8753/414F877A`, constants и helper,
  wave1/gap05 producer tail. Новый ledger объясняет композиционную предпосылку.

## W3-A: разрешённая трёхосевая позиция

- Входы: старая actor_position и target position из N, отдельные tolerance
  и optional fallback-step observations для X/Y/Z.
- Каждый вызов: divisor bits `0x3DAE147B`, bounds ±FLT_MAX, requested_step -1.
  Tolerance — явное наблюдение, читаемое caller перед каждым helper.
- Выход: все три разрешённых компонента или explicit unresolved axis.
  Функция сама не меняет N/actor/output, не вызывает consumer и не решает guards.
- Existing consumer принимает результат через `resolved_position` только
  в явно смоделированном сценарии соответствующей ветки. Обход/ранние выходы
  consumer сохраняются и не требуют ненужных observations.
- Тесты: разные fallback/tolerance на каждой оси, early equality/unordered,
  missing observations на каждой оси, NaN/±0, композиция с текущим consumer.
- Источник: wave2/gap04 caller `414FD938..414FDA4C`. Никаких claims о trace.

## W3-S и W3-V

- Source: исследовать один bounded кандидат N+14, подтвердить объект, ширину,
  направление потока и байты; отрицательный поиск не отсутствие писателя.
  При недостатке данных — точный frontier, без новой функции-заглушки.
- Repro: реально запустить baseline check с внешним HANDOFF и своим build;
  отделить доступность исходного bundle от доступности исходников в Git.
  Не устанавливать unrelated tools и не добавлять CI, требующий публичных DLL.

## Критерии приёмки

- Native и ASan/UBSan с leak detection и halt-on-error; строгие warnings.
- Байтовые якоря и fresh decode для новых статических утверждений, pinned SHA.
- Проверенные unknown/NaN/zero/boundary случаи; прежние тесты не ослабляются.
- Независимые worktree подтверждены, diff чист от чужих файлов и секретов.
- Два рецензента волны 4 проверяют зафиксированный интегрированный SHA.
- Финальный `make check` и целостность input/work проходят на отправленном коде.
- Полные задачи не закрываются по одному локальному helper/инвентарю.

# Изолированные волны 3–4

Пользователь подтвердил баланс, несколько волн и PR без слияния.
План/spec: `PLAN_NEXT.md`, `SPEC_NEXT.md`, коммит `775ac62`.
Все агенты запускаются с `gpt-6.1-sol`; high/fast отдельно не выставляются.

## Изоляция волны 3

Основной checkout: `/tmp/hoplite/workspace`, ветка `hoplite/sikyon-46eee495`.
Общая исходная точка четырёх worktree: `775ac620349bc7ce5c9fd1b56d219135c3c00901`.
Префикс путей: `/tmp/hoplite/worktrees/`; префикс веток: `hoplite/sikyon-46eee495--`.

| Задание | Агент | Worktree | Суффикс ветки |
|---|---|---|---|
| W3-M | w3-marker-worktree | sikyon-w3-marker | w3-marker |
| W3-A | w3-actor-worktree | sikyon-w3-actor | w3-actor |
| W3-S | w3-source-worktree | sikyon-w3-source | w3-source |
| W3-V | w3-repro-worktree | sikyon-w3-repro | w3-repro |

`git worktree list --porcelain` подтверждает отдельные checkout/ветки.
Общие Git objects не означают общий index или исходники. Каждый агент пишет
только свою область, создаёт отдельный коммит и не публикует ветку.
Handoff `/tmp/hoplite/workspace/runs/handoff/RAGEBOT_HANDOFF_V10` доступен
как общий read-only источник по контракту; сборки и scratch раздельные.

## Интеграция и публикация

Координатор проверяет diff и команды, переносит принятые коммиты в основную
ветку, после чего создаёт два новых review-worktree от общего SHA. Конкретные
SHA и результаты записываются после фактической проверки. Ни одна локальная
сборка не объявляется воспроизведением оригинального executable.

Предыдущие результаты до `d0500c5` и план `775ac62` уже отправлены в текущую
ветку. Новые архивы/DLL/build не включаются; архивы, уже находящиеся в main,
не добавлены этой работой. PR не сливается, auto-merge отключается.

## Статус

Волна 3 выполняется. Волна 4 ожидает интегрированный commit.
Итоговый ledger и проверка будут в `evidence/wave3/verification.json`
и `evidence/wave4/`. Эти будущие результаты пока не заявлены как PASS.

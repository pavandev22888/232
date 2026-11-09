# W6: ограниченная офлайн-реконструкция

Фиксированный интегрированный код: `d99dd375160c7fccfd489598942232430d78da2f`.
Ограниченный scope принят; полные GAP не закрыты. Статус, команды, exit codes и hashes логов — в
[verification.json](verification.json); очищенная выборка — [verification.log](verification.log).
Это не полное восстановление Ragebot, не установленный исходный язык/ABI
и не внешний аудит. C++17 сохраняется как regression baseline.

| Участок | Новый локальный результат | Точная оставшаяся граница |
|---|---|---|
| [Callback](aggregate/contract.json) | Non-direct score gates с сохранением INC; tail statistics, initial 0x50-byte copy при C0=0 | Settings provider, handler 4149EC2D, ranking при ненулевом C0, полный selector |
| [Reset](reset/reset_predecessor_evidence.json) | ADD/CMP/JNE и переход в существующую 11-store проекцию | EAX provider, полный цикл/entry, четыре prefix stores, scheduling |
| [Release](modes/contract.json) | Локальные route/stores/increment на независимых post-call observations | Helpers/indirect dispatch, identity/lifetime, полный автомат DT/HS |
| [History](history/README.md) | 21 ordered raw-word copy, local DWORD observation в little-endian и zero store | Suitability/slot identity, неизвестные calls, matrix path и полный selector |
| [Message](message/byte_anchors.json) | Байтовые caller/callee anchors, без нового C++ consumer | FS/register-jump и runtime dereference 5325D3E8 вне образа |
| [Geometry](geometry/contract.md) | Caller inputs/local effects и отдельные candidate windows, без C++ provider | Binding/ABI для 289A6D60; 10876D60 и 103E6D60 не объединены |
| Damage | Реализации, тестов или новых доказательств нет | Задание не выполнено; это не доказанный source blocker |

## Проверки и исправления

- Общий `make check` включает 13 native и 13 ASan/UBSan программ, 13 Python
  unit tests и шесть новых W6 static gates. Число проверок не измеряет готовность.
- Проверки bundle/input/work охватывают 960 файлов / 64 087 записей; шесть
  исходных ZIP совпадают с закреплёнными hashes. Это целостность, не эквивалентность.
- Единственное первоначальное callback-замечание отозвано после повторного
  свежего CFG decode: были смешаны соседние ветви. [Исходный отчёт и resolution](review-callback/README.md)
  сохранены отдельно; код не изменялся под ошибочный вывод.
- Reset initial failure сохранён в `reset/verification_initial_failure.log`.
  `--insn-width=16` устранил обрезание длинных инструкций; проверки не ослаблены.
- History DWORD observation сериализуется явно little-endian; exact-byte
  тест добавлен до интеграционного check. Старый full-check.log этого исполнителя
  предшествует узкому исправлению; интеграционный gate `d99dd37` и свежий
  post-correction gate ниже записаны отдельно.
- Оригинальные DLL не загружались, не исполнялись и не эмулировались.
  Нет подключения к игре, injection или live sender. Исходные материалы read-only.

CODE01/GAP01–GAP06 остаются open/partial; R02/R03/R04, I01/FULL01 blocked.
Supplemental pefile leaf №5 не закрывает old H01 producer/protocol. Следующее
назначение брать по актуальным TASKS/STATUS, не повторяя завершённые локальные slices.
PR #1 остаётся без merge, merge queue и auto-merge.

## Финальная ограниченная приёмка

Code review W6RC-001 отозван рецензентом: два последовательных `memcpy` идут
через отдельный локальный `word`; overlap между views не означает overlap
внутри одного вызова. [Resolution](review-integrated-code/resolution.json)
сохранён отдельно от первоначального отчёта, код/тесты не менялись.

Evidence F1 исправлен в документации: `[ebp-0x38]` читается в `0x413F71A1`,
затем записывается в `+0x3C` в `0x413F71A4`. Независимый
[follow-up](review-integrated-evidence/followup/report.json) на `c8a78c9`
подтвердил байты, адреса и неизменность кода/тестов/static gates относительно `d99dd37`.

Свежий полный coordinator gate на чистом `b869125` после исправления документации:
все четыре команды — exit 0; 13 native и 13 sanitizer программ, 13 Python-тестов,
шесть W6 static gates, 960 bundle-файлов и 64 087 input/work записей.
Исторический прогон `d99dd37` сохранён отдельно. После коммита этих результатов
публикуемый HEAD проходит те же четыре команды; точные SHA/хеши этого финального
прогона фиксируются в PR #1, без самоссылки verification-коммита.

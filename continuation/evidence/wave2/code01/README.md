# CODE01 — baseline review и согласованные pipeline-тесты

Рецензент проверял baseline `ed9df79c028e63ca56fc191a226f5fe2f221df64`,
а не меняющийся код других агентов. `review_baseline.py` берёт код через
`git show`, сверяет 13 файлов исходного инвентаря и 10 байтовых опор.
При `--run-tests` собирает baseline и отдельный probe во временном каталоге;
не исполняет DLL и не меняет handoff. Для проверки нужна доступность baseline
в локальной истории Git.

Подтверждённого production-багa не заявлено. Найдены два пробела покрытия:
локальные fixtures независимо задавали status читателя и AL потребителя,
а произведённые zero/quiet NaN не проходили всю связку reader → actor.
Такие fixtures допустимы для изолированных контрактов, но не доказывают
согласованность исходного вызова.

`tests/test_position_pipeline.cpp` добавляет 20 согласованных синтетических
сценариев / 392 assertions: producer → reader → actor, policy и оба bypass,
source failure, граница возраста 4, zero/quiet NaN и знаковые markers.
AL задаётся по подтверждённым normal-return ветвям читателя, не по наличию
указателя. Старые локальные fixtures не ослаблены и не удалены.

В charge-тестах baseline 2736 проверок используют структурно похожую
reference-формулу: это regression coverage, не независимая эквивалентность.
Отдельный рецензент также не является внешним независимым аудитом.

```sh
python3 -B continuation/evidence/wave2/code01/review_baseline.py
make -C continuation check
```

Первый скрипт намеренно сохраняет baseline-имена API. В текущем модуле
координатор заменил trace/geometry-имена на provider/adjustment после
подтверждения скалярной природы helper; pipeline-тест не меняет смысл.
Результаты интеграционной приёмки — `../verification.json` и `.log`.
CODE01 остаётся открытым: агрегация и выбор цели не реализованы.

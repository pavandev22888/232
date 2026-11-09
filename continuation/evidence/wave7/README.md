# W7: продолжение локальных зависимостей

**Приёмка интеграции пока не завершена. Полный Ragebot не восстановлен.**
Назначения и разделение областей записи: [assignment](assignment.json) и
[актуальный реестр](../../../docs/TASKS.json). База —
`24aca6a6fdb8d5f20da528dc658d3d62baa1e133`; старые W6 evidence не переписываются.

## Подготовленные координатором участки

- [Ranking](rank/contract.json): normal-tail `0x4149E9BD..0x4149EC2D`, порядок
  сравнений, unsigned magnitude DWORD-разности, FP unordered и raw replacement
  0x50 байт. Observation `owner+4→+B0` не заменяет runtime provider.
- [Локальная ABI-связь](abi/contract.json): `0x414A8D68→0x4149E790`, ECX/EDX,
  пролог и обычный эпилог. Не доказан исходный язык, source calling-convention
  keyword, RTTI-владелец или полный ABI.

Reset, history resolver, movement и damage назначены отдельно. До receipt
интеграции их нельзя считать принятыми только по этому документу.

## Неизменные границы

CODE01/GAP01–GAP06, R02/R03/R04, I01/FULL01 не закрываются фактом локальной
приёмки. Исходный язык, runtime providers, полный selector/geometry/damage,
identity/lifetime/scheduling, старый H01 protocol и внешний аудит остаются
отдельными требованиями. Закрытый bounded scope не равен полному модулю.

Только статические материалы и собственный офлайн-код. Оригинальные DLL
не загружаются, не запускаются и не эмулируются. PR только для просмотра:
без merge, очереди и auto-merge.

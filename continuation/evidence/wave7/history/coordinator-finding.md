# Coordinator pre-acceptance finding and correction

The review identified that resolver EDI is both a cursor and an input to a separate masked table index, but those values are not interchangeable. At `0x413F891A`, `DEC EDI` forms the cursor. At `0x413F891B`, `AND EBX, EDI` places the masked table index in EBX while preserving EDI. At `0x413F899A`, `MOV [ESI+0x0C], EDI` stores the unmasked cursor. The previous model stored the table index instead.

Regression case: capacity `5` gives mask `4`; old cursor `4` yields selected EDI cursor `3`, table index EBX `3 & 4 == 0`, final field `+0x0C == 3`. The fix models both values separately and does not assume power-of-two capacity. `verify_source_bytes.py` now pins the DEC/AND instructions and final EDI store.

The consumer now requires a separately supplied `HistoryPostCallSlotObservation`; it does not silently reuse the pre-opaque resolver result. This is an offline evidence boundary, not a claim that the opaque provider mutates or preserves the slot. Resolver initialization also documents stable, non-aliasing storage for the adapter view, entry descriptors, and selected record; those properties are not established for the original object.

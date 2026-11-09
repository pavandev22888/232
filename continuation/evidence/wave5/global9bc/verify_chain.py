#!/usr/bin/env python3
"""Read-only byte checks for the single W5-GLOBAL9BC indirect-store frontier."""

import argparse
import hashlib
import json
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
sys.path.insert(0, str(REPO / 'continuation/evidence/wave2/gap01'))
from verify_gap01_wave2 import BASE, IMAGE_SHA256, decode, normalize, require

FLAG = 0x425009BC
MASK = (1 << 32) - 1


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--handoff', type=Path, required=True)
    args = parser.parse_args()
    ledger = json.loads((HERE / 'chain.json').read_text())
    require(digest(HERE / 'hypothesis.json') == ledger['hypothesis_sha256'],
            'Pre-investigation hypothesis changed')
    hypothesis = json.loads((HERE / 'hypothesis.json').read_text())
    require(hypothesis['hypothesis_id'] == ledger['hypothesis']
            == 'H3_TLS_COUNTER_STORE', 'Candidate differs')
    image = args.handoff / ledger['image']['path']
    raw = image.read_bytes()
    require(hashlib.sha256(raw).hexdigest() == IMAGE_SHA256
            == ledger['image']['sha256'], 'Image hash differs')
    require(len(raw) == ledger['image']['size'], 'Image size differs')
    require(int(ledger['image']['conditional_base'], 16) == BASE, 'Base differs')
    for name, expected in ledger['repository_sources'].items():
        require(digest(REPO / name) == expected, f'Repository source changed: {name}')
    sources = {}
    for name, expected in ledger['saved_sources'].items():
        path = args.handoff / name
        require(digest(path) == expected, f'Saved source changed: {name}')
        sources[name] = json.loads(path.read_text())

    anchors = {}
    saved_count = 0
    for block in ledger['blocks']:
        start, end = (int(block[k], 16) for k in ('start_va', 'end_va_exclusive'))
        require(hashlib.sha256(raw[start - BASE:end - BASE]).hexdigest()
                == block['sha256'], f"Window changed: {block['id']}")
        rows = [{**row, 'instruction': normalize(row['instruction'])}
                for row in block['rows']]
        require(decode(image, start, end) == rows, f"Fresh decode differs: {block['id']}")
        cursor = start
        for row in rows:
            va, payload = int(row['va'], 16), bytes.fromhex(row['bytes'])
            require(va == cursor, f'Noncontiguous selected sequence at {va:#x}')
            require(raw[va - BASE:va - BASE + len(payload)] == payload,
                    f'Bytes changed at {va:#x}')
            cursor += len(payload)
            require(va not in anchors, f'Duplicate anchor {va:#x}')
            anchors[va] = row
            if block['saved_source']:
                saved = sources[block['saved_source']]['instructions']
                require(any(s.get('va') == row['va'] and s.get('bytes') == row['bytes']
                            and normalize(s.get('instruction', '')) == row['instruction']
                            for s in saved), f'Saved row differs at {va:#x}')
                saved_count += 1
        require(cursor == end, f"Selected window incomplete: {block['id']}")

    def at(va, instruction):
        row = anchors[va]
        require(row['instruction'] == normalize(instruction), f'Operand differs at {va:#x}')
        return bytes.fromhex(row['bytes'])

    argument = int.from_bytes(at(0x4148DEC0, 'push 0x425009b8')[1:], 'little')
    at(0x4148DEC5, 'call 0x41da7ce4')
    at(0x41DA7CF4, 'mov ecx,DWORD PTR ds:0x422626ec')
    at(0x41DA7CFA, 'mov eax,DWORD PTR [ebp+0x8]')
    at(0x41DA7CFD, 'inc ecx')
    at(0x41DA7CFE, 'mov DWORD PTR ds:0x422626ec,ecx')
    at(0x41DA7D05, 'mov DWORD PTR [eax],ecx')
    at(0x41DA7D07, 'mov eax,fs:0x2c')
    at(0x41DA7D0D, 'mov ecx,DWORD PTR ds:0x42518540')
    at(0x41DA7D13, 'mov ecx,DWORD PTR [eax+ecx*4]')
    at(0x41DA7D16, 'mov eax,ds:0x422626ec')
    store = at(0x41DA7D1B, 'mov DWORD PTR [ecx+0x8],eax')
    displacement = int.from_bytes(store[2:], 'little', signed=True)
    require(displacement == 8, 'Selected destination displacement differs')
    require(not argument <= FLAG <= argument + 3, 'Old argument store overlaps BC')
    for va in (0x41595497, 0x415DECEE):
        at(va, 'mov BYTE PTR ds:0x425009bc,0x0')
    at(0x4148CBF0, 'or al,BYTE PTR ds:0x425009bc')
    at(0x4147ADAE, 'cmp BYTE PTR ds:0x425009bc,0x0')

    cases = ledger['address_and_value_relation']['overlap_cases']
    require(len(cases) == 4, 'Missing DWORD overlap case')
    for j, case in enumerate(cases):
        q = (FLAG - displacement - j) & MASK
        require(int(case['Q'], 16) == q and case['byte_index_in_W'] == j,
                'Alias/byte relation differs')
        require(int(case['write_start'], 16) == (q + displacement) & MASK
                and int(case['write_end_inclusive'], 16) == (q + displacement + 3) & MASK,
                'Write extent differs')
        require(case['nonzero_condition'] == f'((W >> {8*j}) & 0xff) != 0',
                'Little-endian nonzero condition differs')
    require({int(c['Q'], 16) for c in cases} == set(range(0x425009B1, 0x425009B5)),
            'Alias set differs')
    for cell in ledger['fixed_image_cells']:
        offset, payload = int(cell['va'], 16) - BASE, bytes.fromhex(cell['bytes'])
        require(raw[offset:offset + len(payload)] == payload,
                f"Fixed snapshot cell changed: {cell['role']}")
    require(raw[:2] == b'\0\0', 'Raw-image prefix differs')
    require(ledger['frontier']['positive_sources_proved'] == 0
            and ledger['frontier']['status'] == 'unresolved', 'Frontier overstated')
    print(json.dumps({
        'passed': True, 'task': ledger['task'], 'hypotheses_checked': 1,
        'image_sha256': IMAGE_SHA256, 'fresh_decoded_instructions': len(anchors),
        'saved_caller_rows_rechecked': saved_count, 'overlap_cases': cases,
        'positive_sources_proved': 0, 'G425009BC': 'open',
        'scope': ledger['verification_scope'],
        'original_loaded_executed_or_emulated': False,
    }, indent=2))


if __name__ == '__main__':
    main()

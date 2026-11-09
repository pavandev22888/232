#!/usr/bin/env python3
"""Static provenance and exact-byte gate for one W8 raw queue context chain."""

import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path


def file_hash(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify(handoff):
    anchors = json.loads(Path(__file__).with_name('anchors.json').read_text())
    source = anchors['source']
    data = (handoff / source['path']).read_bytes()
    if len(data) != source['size'] or hashlib.sha256(data).hexdigest() != source['sha256']:
        raise ValueError('nl.bin size/hash mismatch')
    archive = handoff / source['archive_path']
    if file_hash(archive) != source['archive_sha256']:
        raise ValueError('source archive hash mismatch')
    with zipfile.ZipFile(archive) as bundle:
        with bundle.open(source['archive_member']) as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != source['sha256']:
                raise ValueError('restored source differs from archive member')
    base = int(source['image_base'], 16)

    def read(va, count):
        offset = va - base
        if offset < 0 or offset + count > len(data):
            raise ValueError(f'out-of-snapshot address {va:#x}')
        return data[offset:offset + count]

    checked = []
    for region in anchors['regions']:
        expected = bytes.fromhex(region['bytes'])
        if len(expected) != region['size']:
            raise ValueError('anchor size mismatch')
        if hashlib.sha256(expected).hexdigest() != region['sha256']:
            raise ValueError('anchor hash mismatch')
        va = int(region['va'], 16)
        if read(va, len(expected)) != expected:
            raise ValueError(f'byte mismatch at {va:#x}')
        checked.append({k: region[k] for k in ('label', 'va', 'size', 'sha256')})
    for navigation in anchors['archive_navigation']:
        path = handoff / navigation['path']
        if path.stat().st_size != navigation['size'] or file_hash(path) != navigation['sha256']:
            raise ValueError('archive navigation hash mismatch')

    def word(va):
        return struct.unpack('<I', read(va, 4))[0]

    def branch(va, opcode, width, target):
        prefix = bytes.fromhex(opcode)
        if read(va, len(prefix)) != prefix:
            raise ValueError(f'branch opcode mismatch at {va:#x}')
        displacement = struct.unpack('<b' if width == 1 else '<i',
                                     read(va + len(prefix), width))[0]
        actual = va + len(prefix) + width + displacement
        if actual != target:
            raise ValueError(f'branch target mismatch at {va:#x}')
        return {'site': hex(va), 'target': hex(actual)}

    branches = [branch(*edge) for edge in (
        (0x4148FDE3, 'e8', 4, 0x414B2930),
        (0x414B299D, '0f8e', 4, 0x414B2AB8),
        (0x414B29ED, '74', 1, 0x414B2A44),
        (0x414B29F1, 'e8', 4, 0x41DA7B7C),
        (0x414B2A1E, '75', 1, 0x414B29D0),
        (0x414B2A4F, '76', 1, 0x414B2A95),
        (0x414B2AA3, 'e8', 4, 0x414AC330),
        (0x414B2AB4, '75', 1, 0x414B2AA0),
        (0x414B2AC0, '76', 1, 0x414B2B05),
        (0x414AC2DC, '76', 1, 0x414AC322),
        (0x414AC2E0, 'eb', 1, 0x414AC2F4),
        (0x414AC2F2, '73', 1, 0x414AC322),
        (0x414AC308, '76', 1, 0x414AC2F0),
        (0x414AC31E, '72', 1, 0x414AC310),
        (0x414AC320, 'eb', 1, 0x414AC2F0),
        (0x414AC385, '7f', 1, 0x414AC3A9),
        (0x414AC3BD, '75', 1, 0x414AC387),
    )]
    checks = (
        (word(0x4148FDBF), 0xFFFFFD64, 'capture displacement'),
        (word(0x4148FDC3), 0x414AD910, 'opaque callback literal'),
        (word(0x4148FDC9), 0xFFFFFF64, 'callback context displacement'),
        (word(0x4148FDD5), 0xFFFFFD64, 'ECX capture displacement'),
        (word(0x4148FDDF), 0x4223908C, 'slot array address'),
        (word(0x414B29FB), 0x42078104, 'primary task table'),
        (word(0x414B2A02), 0x420780F0, 'secondary task table'),
        (word(0x414B2A09), 1, 'initial task count word'),
        (word(0x414B2A10), 0x414AC2D0, 'worker literal'),
        (word(0x42078110), 0x4149BFC0, 'primary table slot +C'),
        (word(0x42078108), 0x41351F10, 'primary table slot +4'),
    )
    for actual, expected, name in checks:
        if actual != expected:
            raise ValueError(f'{name} mismatch: {actual:#x}')
    if read(0x414AC2F9, 5) != bytes.fromhex('f00fc15e08'):
        raise ValueError('worker LOCK XADD +8 mismatch')
    if read(0x414AC392, 6) != bytes.fromhex('ff15f0575042'):
        raise ValueError('opaque wait binding operand mismatch')
    if read(0x414B29DF, 3) != bytes.fromhex('ff5250'):
        raise ValueError('opaque enqueue operand mismatch')
    return {
        'result': 'passed', 'task': anchors['task'], 'source': source,
        'regions': checked, 'direct_branches': branches,
        'lifetime': 'unresolved: indirect enqueue/wait providers and ownership',
        'original_loaded_run_or_emulated': False,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--handoff', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.handoff), indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

#!/usr/bin/env python3
"""Static source gate. Reads bytes/ZIP members only; never loads original code."""
import argparse
import hashlib
import io
import json
from pathlib import Path
from zipfile import ZipFile

RAW_BASE = 0x412A0000
NL_SHA256 = '682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e'
DLL_SHA256 = 'c1ce8c6d6f1172114d3cff6efe147903292be5eaa6e1c2e28495440fa8f7982c'
NL_ZIP_SHA256 = 'cc7fcb0e3a34d1f5b3adf884e889e9458c02158ee9823a0c45e161786920dd49'
RECOVERED_ZIP_SHA256 = 'e9e887ec69b49fa75d3dbfef67c25cdf15055c5019658ef52860e9d20e09f7fb'
WINDOWS = {
    0x413F3890: (0x19F, '3197f4ffc416400aead67f2e07fb2f9a972735cb675068f8281bbb28d40131a5'),
    0x413F88F0: (0xC0, 'b8d8206f38ea7844a1db06850dd6d6a098b2fc0e989946a045ca26ecff200a92'),
    0x4200C9A0: (0x574, 'e0adef6a3c41b7d1197ddf0d70a2ad433ab76bf71e975e23817641c5533ec279'),
    0x4200CF20: (0x15A, 'a926212d42a287f420bd22d103ff89eb9f842d37a088c203a60a3d8c9b44c978'),
}
DECOMPILATIONS = {
    'functions/4200C9A0.c': '96e30f6e153f3acab209463625dd23252a97d5ac3725f208b3e1babd56cd71e7',
    'functions/4200CF20.c': '8e7a5e9f4b98f2e42194351dedbf8a37234e9e103eee7d2588810b0591427218',
}


def require(condition, label):
    if not condition:
        raise SystemExit('FAIL ' + label)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def source_bytes(image, va, size):
    offset = va - RAW_BASE
    require(offset >= 0 and offset + size <= len(image), f'range {va:#x}+{size:#x}')
    return image[offset:offset + size]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--handoff', required=True, type=Path)
    args = parser.parse_args()
    workspace = args.handoff / 'workspace'
    image = (workspace / 'work/nl_handoff_2026-11-06/nl.bin').read_bytes()
    dll = (workspace / 'input/dll/neverlose.dll').read_bytes()
    require(digest(image) == NL_SHA256 and len(image) == 0x3501000, 'nl.bin hash/size')
    require(digest(dll) == DLL_SHA256, 'DLL hash')
    require(dll[0x629B10:0x629B10 + 0x3501000] == image, 'DLL resource != nl.bin')
    print(f'PASS nl.bin sha256={NL_SHA256}; DLL sha256={DLL_SHA256}; resource equality')

    source_zip = (workspace / 'input/nl_handoff_2026-11-06.zip').read_bytes()
    require(digest(source_zip) == NL_ZIP_SHA256, 'source ZIP hash')
    with ZipFile(io.BytesIO(source_zip)) as archive:
        require(archive.read('nl.bin') == image, 'source ZIP nl.bin member')
    recovered_zip = (workspace / 'input/recovered_nl.zip').read_bytes()
    require(digest(recovered_zip) == RECOVERED_ZIP_SHA256, 'recovered ZIP hash')
    with ZipFile(io.BytesIO(recovered_zip)) as archive:
        functions_zip = archive.read('recovered_nl/functions.zip')
    restored = workspace / 'work/recovered_nl/recovered_nl/functions.zip'
    require(functions_zip == restored.read_bytes(), 'nested/restored functions.zip equality')
    with ZipFile(io.BytesIO(functions_zip)) as functions:
        for name, expected in DECOMPILATIONS.items():
            require(digest(functions.read(name)) == expected, name + ' decompilation hash')
            print(f'PASS reference-only ZIP member {name} sha256={expected}')
    print('PASS source ZIP bytes and nested/restored functions.zip equality')

    evidence = json.loads(Path(__file__).with_name('source_bytes.json').read_text())
    require(int(evidence['raw_base'], 16) == RAW_BASE, 'raw base')
    require(len(evidence['windows']) == len(WINDOWS), 'window count')
    seen = set()
    for window in evidence['windows']:
        va = int(window['va'], 16)
        require(va in WINDOWS and va not in seen, 'window address/uniqueness')
        seen.add(va)
        size, expected = WINDOWS[va]
        require(int(window['size'], 16) == size, 'window size')
        require(int(window['file_offset'], 16) == va - RAW_BASE, 'file offset')
        actual = source_bytes(image, va, size)
        require(digest(actual) == expected == window['sha256'], f'{va:#x} hash')
        require(actual == bytes.fromhex(window['bytes']), f'{va:#x} explicit bytes')
        print(f'PASS VA={va:#x} size={size:#x} sha256={expected}')
    for anchor in evidence['anchors']:
        va = int(anchor['va'], 16)
        expected = bytes.fromhex(anchor['bytes'])
        require(source_bytes(image, va, len(expected)) == expected, anchor['label'])
    for address, targets in [(0x4200CC04, [0x4200CC14, 0x4200CC1C, 0x4200CC28, 0x4200CC3C]),
                             (0x4200CCB0, [0x4200CCC0, 0x4200CCC8, 0x4200CCD8, 0x4200CCEC])]:
        expected = b''.join(target.to_bytes(4, 'little') for target in targets)
        require(source_bytes(image, address, 16) == expected, f'{address:#x} jump table data')
    print('PASS growth/reload/copy-order/release/publication/cursor/consumer anchors and jump-table data')
    print('BOUNDARY allocation/release/failure calls, null-slot allocator and post-provider identity remain unresolved')


if __name__ == '__main__':
    main()

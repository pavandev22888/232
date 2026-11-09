#!/usr/bin/env python3
import argparse
import hashlib
import struct
import sys
import zipfile
from pathlib import Path

NL_ARCHIVE = "workspace/input/nl_handoff_2026-11-06.zip"
NL_ENTRY = "nl.bin"
CLIENT_DLL = "workspace/input/dll/client.dll"
EXPECTED = {
    "nl_archive_size": 149819978,
    "nl_archive_sha256": "cc7fcb0e3a34d1f5b3adf884e889e9458c02158ee9823a0c45e161786920dd49",
    "nl_size": 55578624,
    "nl_sha256": "682ce73b371349f87da6fd9365f8550e240dcdb97951e80445e70286275ad96e",
    "client_size": 16377192,
    "client_sha256": "183d42fac8a4ecc24dc02edf5f09122f37fe9e7533ca093a867718b25cfdeb03",
}
CALLER = (0x253C90, 0x253EB5, "72bab48ea792c8f4a906af85efe348288aaebaef9169c482df6fc596f83595cb")
CALL_SETUP = (0x253E19, 0x253E5A, "2cc0b6ecc5100b19b3d6981c3d7c8505e4eb5fecc9a4c01766abd2776416fe0f")
RESULT_EFFECT = (0x253E5D, 0x253EB5, "58dbf01250553f893287c7993997ca2d910a094a3730bf2939dd74f0dfc989a5")
THRESHOLD_OFFSET = 0xDBA554
CANDIDATES = (
    (0x10876D60, 0x876160, "3e8684c72ca46e7023c0dca0b703cca4ebb574f8986c3ca3355c7c26d77564b4"),
    (0x103E6D60, 0x3E6160, "4d1e06c23eb46d58717bdaf3fde7364c7bb646fb22fb2d235247ebcb623ceea6"),
)
MASK32 = 0xFFFFFFFF


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def hash_file(path):
    digest = hashlib.sha256()
    size = 0
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            size += len(chunk)
            digest.update(chunk)
    return size, digest.hexdigest()


def rol32(value, count):
    return ((value << count) | (value >> (32 - count))) & MASK32


def ror32(value, count):
    return ((value >> count) | (value << (32 - count))) & MASK32


def pe_map_rva(image, rva):
    pe_offset = struct.unpack_from("<I", image, 0x3C)[0]
    require(image[pe_offset:pe_offset + 4] == b"PE\0\0", "PE signature")
    section_count = struct.unpack_from("<H", image, pe_offset + 6)[0]
    optional_size = struct.unpack_from("<H", image, pe_offset + 20)[0]
    optional = pe_offset + 24
    require(struct.unpack_from("<H", image, optional)[0] == 0x10B, "PE32 optional header")
    image_base = struct.unpack_from("<I", image, optional + 28)[0]
    section_table = optional + optional_size
    for index in range(section_count):
        entry = section_table + 40 * index
        name = image[entry:entry + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from("<IIII", image, entry + 8)
        if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
            delta = rva - virtual_address
            require(delta < raw_size, "candidate RVA must have file-backed bytes")
            return image_base, name, raw_offset + delta
    raise ValueError("candidate RVA is not in a PE section")


def main():
    parser = argparse.ArgumentParser(description="Verify W6 geometry caller evidence without loading original code.")
    parser.add_argument("--handoff", required=True, type=Path)
    handoff = parser.parse_args().handoff.resolve()

    archive = handoff / NL_ARCHIVE
    archive_size, archive_hash = hash_file(archive)
    require((archive_size, archive_hash) == (EXPECTED["nl_archive_size"], EXPECTED["nl_archive_sha256"]),
            "nl_handoff archive pin")
    with zipfile.ZipFile(archive) as bundle:
        nl = bundle.read(NL_ENTRY)
    require((len(nl), sha256(nl)) == (EXPECTED["nl_size"], EXPECTED["nl_sha256"]), "nl.bin nested pin")
    print("PASS nl_handoff archive and nested nl.bin pins")

    for label, (start, end, expected_hash) in (
        ("caller", CALLER), ("indirect call setup", CALL_SETUP), ("result/effect", RESULT_EFFECT)
    ):
        actual = sha256(nl[start:end])
        require(actual == expected_hash, label + " byte window")
        print("PASS " + label + " byte window SHA-256 " + actual)

    require(nl[THRESHOLD_OFFSET:THRESHOLD_OFFSET + 4] == bytes.fromhex("0000803f"),
            "comparison threshold must be binary32 1.0")
    print("PASS threshold VA 0x4205A554 bytes 00 00 80 3f")

    value = 0x870D2506
    value = rol32(value, 0x0B)
    require(value == 0x69283438, "call target ROL")
    value = (value - 0x2061EBDB) & MASK32
    require(value == 0x48C6485D, "call target SUB")
    value = ror32(value, 0x1A)
    require(value == 0x31921752, "call target ROR")
    value ^= 0x7DA184A9
    require(value == 0x4C3393FB, "call target XOR")
    value = (value - 0x2399269B) & MASK32
    require(value == 0x289A6D60, "computed call target")
    print("PASS fresh caller immediates compute indirect target 0x289A6D60")

    client_path = handoff / CLIENT_DLL
    client_size, client_hash = hash_file(client_path)
    require((client_size, client_hash) == (EXPECTED["client_size"], EXPECTED["client_sha256"]), "client.dll pin")
    client = client_path.read_bytes()
    candidate_bases = []
    for preferred_va, expected_offset, expected_hash in CANDIDATES:
        image_base, section, actual_offset = pe_map_rva(client, preferred_va - 0x10000000)
        require(image_base == 0x10000000, "client preferred image base")
        require(section == ".text" and actual_offset == expected_offset, "candidate PE section mapping")
        sample = client[actual_offset:actual_offset + 96]
        require(sha256(sample) == expected_hash, "candidate sample bytes")
        candidate_bases.append(0x289A6D60 - (preferred_va - image_base))
    require(candidate_bases == [0x28130000, 0x285C0000], "separate hypothetical candidate bases")
    require(candidate_bases[0] != candidate_bases[1], "candidate bases must remain distinct")
    print("PASS separate client candidate RVA/raw windows; neither runtime base is established")
    print("UNRESOLVED provider identity and runtime binding at 0x289A6D60")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, zipfile.BadZipFile, struct.error) as error:
        print("FAIL:", error, file=sys.stderr)
        sys.exit(1)

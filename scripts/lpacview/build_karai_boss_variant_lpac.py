#!/usr/bin/env python3
"""Append boss Karai (enemy084) assets to the player Karai LPAC as unique
"karai_boss" records, losslessly.

Source inputs come from the verified US/NA retail TMNT.DAT (AFS):
  * entry 145: player Karai LPAC (49 chunks)
  * entry 87:  stage LPAC containing boss Karai (enemy084) records

All 49 original player Karai chunks are kept byte-for-byte in place. Appended:
  TEXTURE_DICTIONARY karai_boss   (payload of enemy084 texture dictionary)
  MODEL_TOON         karai_boss   (enemy084 TDFF; extra header texture set -> karai_boss)
  MOTION_SET         karai_boss   (enemy084 motion-set header, parent unchanged)
  MOTION x39                      (all enemy084 motions, unchanged)
  MOTION_PARAMETER   karai_boss   (enemy084 motion parameters, unchanged payload)
Only chunk names (and the model's texture-set reference) differ from the sources.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import dataclass
from pathlib import Path

ALIGNMENT = 64
NEW_NAME = "karai_boss"
BOSS_NAME = "enemy084"

PLAYER_ENTRY = 145
PLAYER_SIZE = 614784
PLAYER_SHA256 = "d2bbaeefff2b15bb92484300087553ce909afded10502ac2bdd61d4f6f501c7d"
PLAYER_CHUNKS = 49
BOSS_ENTRY = 87
BOSS_SIZE = 2144448
BOSS_SHA256 = "93d96c18442234476a0690315c11713cf6c7734643d57aa892aa77203dc34884"
DAT_SIZE = 502067200

TYPE_MODEL_TOON = 1
TYPE_MOTION_SET = 2
TYPE_MOTION = 3
TYPE_MOTION_PARAMETER = 4
TYPE_TEXTURE_DICTIONARY = 7


def align(value: int) -> int:
    return (value + ALIGNMENT - 1) & ~(ALIGNMENT - 1)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


@dataclass(frozen=True)
class Chunk:
    name: str
    type: int
    header: bytes
    payload: bytes
    padding: bytes

    @property
    def encoded(self) -> bytes:
        return self.header + self.payload + self.padding


def parse_lpac(data: bytes) -> tuple[bytes, list[Chunk]]:
    if len(data) < ALIGNMENT or data[:4] != b"LPAC":
        raise ValueError("Input is not an LPAC archive")
    count = struct.unpack_from("<I", data, 4)[0]
    chunks: list[Chunk] = []
    position = ALIGNMENT
    for index in range(count):
        header = data[position:position + ALIGNMENT]
        if len(header) != ALIGNMENT:
            raise ValueError(f"Chunk {index} header is truncated")
        chunk_type, size = struct.unpack_from("<II", header, 0)
        name = header[16:32].split(b"\0", 1)[0].decode("ascii")
        start = position + ALIGNMENT
        end = start + size
        nxt = align(end)
        if nxt > len(data):
            raise ValueError(f"Chunk {index} payload is truncated")
        chunks.append(Chunk(name, chunk_type, header, data[start:end], data[end:nxt]))
        position = nxt
    if position != len(data):
        raise ValueError("LPAC has unexpected trailing bytes")
    return data[:ALIGNMENT], chunks


def renamed(chunk: Chunk, name: str, extra_texture_set: str | None = None) -> Chunk:
    encoded = name.encode("ascii")
    if not encoded or len(encoded) >= 16:
        raise ValueError(f"Name must be 1-15 ASCII bytes: {name}")
    header = bytearray(chunk.header)
    header[16:32] = encoded.ljust(16, b"\0")
    if extra_texture_set is not None:
        texture = extra_texture_set.encode("ascii")
        if len(texture) >= 16:
            raise ValueError("Texture set name too long")
        header[32:48] = texture.ljust(16, b"\0")
    return Chunk(name, chunk.type, bytes(header), chunk.payload, chunk.padding)


def read_afs_entry(dat: Path, entry: int, size: int, digest: str) -> bytes:
    with dat.open("rb") as stream:
        if stream.read(4) != b"AFS\0":
            raise ValueError("TMNT.DAT is not an AFS archive")
        count = struct.unpack("<I", stream.read(4))[0]
        if entry >= count:
            raise ValueError("AFS entry index out of range")
        stream.seek(8 + entry * 8)
        offset, length = struct.unpack("<II", stream.read(8))
        if length != size:
            raise ValueError(f"AFS entry {entry} size {length} != expected {size}")
        stream.seek(offset)
        data = stream.read(length)
    if sha256(data) != digest:
        raise ValueError(f"AFS entry {entry} hash mismatch")
    return data


def boss_records(chunks: list[Chunk]) -> list[Chunk]:
    def find(chunk_type: int) -> int:
        hits = [i for i, c in enumerate(chunks) if c.type == chunk_type and c.name == BOSS_NAME]
        if len(hits) != 1:
            raise ValueError(f"Expected exactly one {BOSS_NAME} chunk of type {chunk_type}, found {len(hits)}")
        return hits[0]

    texture = find(TYPE_TEXTURE_DICTIONARY)
    model = find(TYPE_MODEL_TOON)
    motion_set = find(TYPE_MOTION_SET)
    parameter = find(TYPE_MOTION_PARAMETER)
    motions = chunks[motion_set + 1:parameter]
    if not motions or any(c.type != TYPE_MOTION for c in motions):
        raise ValueError("Boss motion block is not contiguous between MOTION_SET and MOTION_PARAMETER")
    if chunks[model].header[32:48].split(b"\0", 1)[0] != BOSS_NAME.encode():
        raise ValueError("Boss model does not reference the enemy084 texture set")
    return (
        [renamed(chunks[texture], NEW_NAME),
         renamed(chunks[model], NEW_NAME, extra_texture_set=NEW_NAME),
         renamed(chunks[motion_set], NEW_NAME)]
        + motions
        + [renamed(chunks[parameter], NEW_NAME)]
    )


def build(player_data: bytes, boss_data: bytes) -> tuple[bytes, dict]:
    player_header, player_chunks = parse_lpac(player_data)
    _, boss_chunks = parse_lpac(boss_data)
    if len(player_chunks) != PLAYER_CHUNKS:
        raise ValueError("Unexpected player Karai chunk count")
    if any(c.name == NEW_NAME for c in player_chunks):
        raise ValueError("Player LPAC already contains karai_boss records")

    added = boss_records(boss_chunks)
    output_chunks = player_chunks + added
    header = bytearray(player_header)
    struct.pack_into("<I", header, 4, len(output_chunks))
    output = bytes(header) + b"".join(c.encoded for c in output_chunks)

    # Validation: lossless original chunks, exact appended payloads.
    _, check = parse_lpac(output)
    if len(check) != len(output_chunks):
        raise AssertionError("Chunk count mismatch after rebuild")
    for i, chunk in enumerate(player_chunks):
        if check[i].encoded != chunk.encoded:
            raise AssertionError(f"Original chunk {i} changed")
    for j, chunk in enumerate(added):
        got = check[PLAYER_CHUNKS + j]
        if got.payload != chunk.payload or got.type != chunk.type:
            raise AssertionError(f"Appended chunk {j} payload/type changed")
    if output[8:ALIGNMENT] != player_data[8:ALIGNMENT]:
        raise AssertionError("LPAC file header changed outside the chunk count")

    report = {
        "output_chunks": len(output_chunks),
        "original_chunks": PLAYER_CHUNKS,
        "appended": [{"name": c.name, "type": c.type, "size": len(c.payload),
                      "payload_sha256": sha256(c.payload)} for c in added],
        "boss_motions": [c.name for c in added if c.type == TYPE_MOTION],
        "player_texture_equals_boss_texture": player_chunks[0].payload == added[0].payload,
        "output_size": len(output),
        "output_sha256": sha256(output),
    }
    return output, report


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dat", required=True, type=Path, help="Untouched US retail TMNT.DAT (read only)")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    if args.dat.stat().st_size != DAT_SIZE:
        raise ValueError("TMNT.DAT is not the US retail size")
    if args.output.exists():
        raise FileExistsError(f"Refusing to overwrite {args.output}")
    player = read_afs_entry(args.dat, PLAYER_ENTRY, PLAYER_SIZE, PLAYER_SHA256)
    boss = read_afs_entry(args.dat, BOSS_ENTRY, BOSS_SIZE, BOSS_SHA256)
    output, report = build(player, boss)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    if args.report:
        args.report.write_text(json.dumps(report, indent=2))
    print(json.dumps({k: v for k, v in report.items() if k != "appended"}, indent=2))


if __name__ == "__main__":
    main()

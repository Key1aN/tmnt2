#!/usr/bin/env python3
"""Splice Slashuur dual-costume assets into the verified final LPAC losslessly."""

from __future__ import annotations

import argparse
import hashlib
import struct
from dataclasses import dataclass
from pathlib import Path


ALIGNMENT = 64
SOURCE_CHUNK_COUNT = 77
OUTPUT_CHUNK_COUNT = 79
SOURCE_SHA256 = "004b8a19dddffe6aeef991a96ea49c33a8ab97ee21a86f26f7eff6a554bbb7fc"


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
        if position + ALIGNMENT > len(data):
            raise ValueError(f"LPAC chunk {index} header is truncated")
        header = data[position : position + ALIGNMENT]
        chunk_type, size = struct.unpack_from("<II", header, 0)
        name = header[16:32].split(b"\0", 1)[0].decode("ascii")
        payload_start = position + ALIGNMENT
        payload_end = payload_start + size
        next_position = align(payload_end)
        if next_position > len(data):
            raise ValueError(f"LPAC chunk {index} payload is truncated")
        chunks.append(
            Chunk(
                name=name,
                type=chunk_type,
                header=header,
                payload=data[payload_start:payload_end],
                padding=data[payload_end:next_position],
            )
        )
        position = next_position

    if position != len(data):
        raise ValueError("LPAC contains unexpected trailing bytes")
    return data[:ALIGNMENT], chunks


def replacement(chunk: Chunk, name: str, payload: bytes) -> Chunk:
    encoded_name = name.encode("ascii")
    if not encoded_name or len(encoded_name) >= 16:
        raise ValueError(f"LPAC name must contain 1-15 ASCII bytes: {name}")

    header = bytearray(chunk.header)
    struct.pack_into("<I", header, 4, len(payload))
    header[16:32] = encoded_name.ljust(16, b"\0")
    return Chunk(
        name=name,
        type=chunk.type,
        header=bytes(header),
        payload=payload,
        padding=bytes(align(len(payload)) - len(payload)),
    )


def require_chunk(chunk: Chunk, index: int, name: str, chunk_type: int) -> None:
    if chunk.name != name or chunk.type != chunk_type:
        raise ValueError(
            f"Unexpected source chunk {index}: {chunk.name!r}, type {chunk.type}; "
            f"expected {name!r}, type {chunk_type}"
        )


def build(source_data: bytes, texture_data: bytes, model_data: bytes) -> bytes:
    source_header, source_chunks = parse_lpac(source_data)
    if len(source_chunks) != SOURCE_CHUNK_COUNT:
        raise ValueError(f"Expected {SOURCE_CHUNK_COUNT} source chunks")
    if sha256(source_data) != SOURCE_SHA256:
        raise ValueError("Source LPAC does not match the verified final Slashuur LPAC")

    require_chunk(source_chunks[0], 0, "player", 16)
    require_chunk(source_chunks[1], 1, "slashuur", 1)
    require_chunk(source_chunks[48], 48, "slashuur", 4)
    if len(model_data) != len(source_chunks[1].payload):
        raise ValueError("Prototype model size must match the original lossless TDFF size")
    if not texture_data:
        raise ValueError("Dual texture dictionary is empty")

    player = replacement(source_chunks[0], "player", texture_data)
    prototype_model = replacement(source_chunks[1], "slashuur_proto", model_data)
    prototype_parameters = replacement(
        source_chunks[48],
        "slashuur_proto",
        source_chunks[48].payload,
    )

    output_chunks = (
        [player, source_chunks[1], prototype_model]
        + source_chunks[2:49]
        + [prototype_parameters]
        + source_chunks[49:]
    )
    if len(output_chunks) != OUTPUT_CHUNK_COUNT:
        raise AssertionError("Internal output chunk count mismatch")

    output_header = bytearray(source_header)
    struct.pack_into("<I", output_header, 4, OUTPUT_CHUNK_COUNT)
    output = bytes(output_header) + b"".join(chunk.encoded for chunk in output_chunks)
    validate_output(source_data, output, texture_data, model_data)
    return output


def validate_output(
    source_data: bytes,
    output_data: bytes,
    texture_data: bytes,
    model_data: bytes,
) -> None:
    _, source_chunks = parse_lpac(source_data)
    _, output_chunks = parse_lpac(output_data)
    if len(output_chunks) != OUTPUT_CHUNK_COUNT:
        raise ValueError("Output LPAC does not contain 79 chunks")

    for source_index, source_chunk in enumerate(source_chunks):
        if source_index < 2:
            output_index = source_index
        elif source_index < 49:
            output_index = source_index + 1
        else:
            output_index = source_index + 2

        output_chunk = output_chunks[output_index]
        if source_index == 0:
            if output_chunk.name != "player" or output_chunk.type != source_chunk.type:
                raise ValueError("Player texture chunk metadata changed unexpectedly")
            if output_chunk.header[8:] != source_chunk.header[8:]:
                raise ValueError("Player texture chunk header changed outside its size")
            if output_chunk.payload != texture_data:
                raise ValueError("Player texture replacement failed")
        elif output_chunk.encoded != source_chunk.encoded:
            raise ValueError(f"Existing source chunk {source_index} changed")

    model = output_chunks[2]
    if (
        model.name != "slashuur_proto"
        or model.type != source_chunks[1].type
        or model.payload != model_data
        or model.header[32:] != source_chunks[1].header[32:]
    ):
        raise ValueError("Prototype model chunk was not cloned safely")

    parameters = output_chunks[50]
    if (
        parameters.name != "slashuur_proto"
        or parameters.type != source_chunks[48].type
        or parameters.payload != source_chunks[48].payload
        or parameters.header[32:] != source_chunks[48].header[32:]
    ):
        raise ValueError("Prototype motion-parameter chunk was not cloned safely")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-lpac", required=True, type=Path)
    parser.add_argument("--texture-dictionary", required=True, type=Path)
    parser.add_argument("--prototype-model", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    if args.output.exists():
        raise FileExistsError(f"Refusing to overwrite: {args.output}")
    output = build(
        args.source_lpac.read_bytes(),
        args.texture_dictionary.read_bytes(),
        args.prototype_model.read_bytes(),
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    print(f"Created {args.output}")
    print(f"Size: {len(output)}")
    print(f"SHA-256: {sha256(output)}")


if __name__ == "__main__":
    main()

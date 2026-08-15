#!/usr/bin/env python3
import argparse
import hashlib
import json
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "fixtures" / "image-input"


def chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(
        ">I", zlib.crc32(kind + payload) & 0xFFFFFFFF
    )


def filtered_rows(rows: list[bytes], channels: int) -> bytes:
    result = bytearray()
    previous = bytes(len(rows[0]))
    for filter_type, row in enumerate(rows):
        encoded = bytearray(len(row))
        for i, value in enumerate(row):
            left = row[i - channels] if i >= channels else 0
            above = previous[i]
            corner = previous[i - channels] if i >= channels else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            else:
                p = left + above - corner
                choices = [(abs(p - left), left), (abs(p - above), above),
                           (abs(p - corner), corner)]
                predictor = min(choices, key=lambda item: item[0])[1]
            encoded[i] = (value - predictor) & 255
        result.append(filter_type)
        result.extend(encoded)
        previous = row
    return bytes(result)


def png(width: int, height: int, color: int, rows: list[bytes],
        mode: str, palette: bytes = b"") -> bytes:
    channels = {0: 1, 2: 3, 3: 1, 6: 4}[color]
    raw = filtered_rows(rows, channels)
    if mode == "stored":
        compressed = zlib.compress(raw, level=0)
    elif mode == "fixed":
        encoder = zlib.compressobj(level=9, strategy=zlib.Z_FIXED)
        compressed = encoder.compress(raw) + encoder.flush()
    else:
        compressed = zlib.compress(raw, level=9)
    result = b"\x89PNG\r\n\x1a\n"
    result += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, color,
                                         0, 0, 0))
    if palette:
        result += chunk(b"PLTE", palette)
    split = len(compressed) // 2
    result += chunk(b"IDAT", compressed[:split])
    result += chunk(b"IDAT", compressed[split:])
    result += chunk(b"IEND", b"")
    return result


def bmp24(top_down: bool) -> bytes:
    width, height = 3, 2
    rows_rgb = [
        [(255, 0, 0), (0, 255, 0), (0, 0, 255)],
        [(255, 255, 255), (0, 0, 0), (64, 128, 192)],
    ]
    rows = rows_rgb if top_down else list(reversed(rows_rgb))
    pixels = bytearray()
    for row in rows:
        for r, g, b in row:
            pixels.extend((b, g, r))
        pixels.extend(b"\0" * 3)
    signed_height = -height if top_down else height
    header = struct.pack("<2sIHHI", b"BM", 54 + len(pixels), 0, 0, 54)
    dib = struct.pack("<IiiHHIIiiII", 40, width, signed_height, 1, 24, 0,
                      len(pixels), 0, 0, 0, 0)
    return header + dib + pixels


def bmp8() -> bytes:
    width, height = 3, 2
    palette = b"".join(bytes((i, i, i, 0)) for i in range(256))
    rows = [bytes((40, 50, 60, 0)), bytes((10, 20, 30, 0))]
    pixels = b"".join(rows)
    offset = 54 + len(palette)
    header = struct.pack("<2sIHHI", b"BM", offset + len(pixels), 0, 0, offset)
    dib = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 8, 0,
                      len(pixels), 0, 0, 256, 0)
    return header + dib + palette + pixels


def solid_bmp24() -> bytes:
    width, height = 3, 5
    row = bytes((0, 0, 255)) * width + b"\0" * 3
    pixels = row * height
    header = struct.pack("<2sIHHI", b"BM", 54 + len(pixels), 0, 0, 54)
    dib = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0,
                      len(pixels), 0, 0, 0, 0)
    return header + dib + pixels


def files() -> dict[str, bytes]:
    rgb_rows = [
        bytes((255, 0, 0, 0, 255, 0, 0, 0, 255)),
        bytes((255, 255, 255, 0, 0, 0, 64, 128, 192)),
        bytes((1, 2, 3, 4, 5, 6, 7, 8, 9)),
        bytes((250, 10, 20, 30, 240, 40, 50, 60, 230)),
        bytes((11, 22, 33, 44, 55, 66, 77, 88, 99)),
    ]
    gray_rows = [bytes((10, 20, 30)), bytes((40, 50, 60))]
    rgba_rows = [bytes((255, 0, 0, 0, 0, 255, 0, 255))]
    solid_rgba_rows = [
        b"".join(bytes((255, 0, 0, (y * 53 + x * 29) & 255))
                 for x in range(3))
        for y in range(5)
    ]
    solid_gray_rows = [bytes((128, 128, 128)) for _ in range(5)]
    palette = bytes((255, 0, 0, 0, 255, 0, 0, 0, 255))
    result = {
        "rgb-filters-dynamic.png": png(3, 5, 2, rgb_rows, "dynamic"),
        "rgb-fixed.png": png(3, 5, 2, rgb_rows, "fixed"),
        "rgb-stored.png": png(3, 5, 2, rgb_rows, "stored"),
        "gray.png": png(3, 2, 0, gray_rows, "dynamic"),
        "rgba.png": png(2, 1, 6, rgba_rows, "dynamic"),
        "solid-rgba.png": png(
            3, 5, 6, solid_rgba_rows, "dynamic"),
        "solid-gray.png": png(
            3, 5, 0, solid_gray_rows, "dynamic"),
        "palette.png": png(3, 1, 3, [bytes((0, 1, 2))], "dynamic",
                           palette),
        "rgb-bottom-up.bmp": bmp24(False),
        "rgb-top-down.bmp": bmp24(True),
        "gray.bmp": bmp8(),
        "solid-color.bmp": solid_bmp24(),
    }
    raw = bytes((5, 7))
    compressed = zlib.compress(raw, level=0)
    invalid_filter = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 0, 0, 0, 0))
        + chunk(b"IDAT", compressed)
        + chunk(b"IEND", b"")
    )
    result["invalid-filter.png"] = invalid_filter
    bad_deflate = bytearray(result["gray.png"])
    idat = bad_deflate.find(b"IDAT")
    bad_deflate[idat + 4] ^= 0x40
    length = struct.unpack(">I", bad_deflate[idat - 4:idat])[0]
    payload_end = idat + 4 + length
    crc = zlib.crc32(bad_deflate[idat:payload_end]) & 0xFFFFFFFF
    bad_deflate[payload_end:payload_end + 4] = struct.pack(">I", crc)
    result["invalid-deflate.png"] = bytes(bad_deflate)
    result["unsupported-depth.png"] = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 16, 0, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(b"\0\0"))
        + chunk(b"IEND", b"")
    )
    result["unsupported-interlace.png"] = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 0, 0, 0, 1))
        + chunk(b"IDAT", zlib.compress(b"\0\0"))
        + chunk(b"IEND", b"")
    )
    result["overflow-dimensions.png"] = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 0xFFFFFFFF, 0xFFFFFFFF,
                                     8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(b""))
        + chunk(b"IEND", b"")
    )
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generated = files()
    manifest = {
        name: {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        for name, data in sorted(generated.items())
    }
    manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) +
                      "\n").encode()
    if args.check:
        for name, data in generated.items():
            if not (FIXTURES / name).is_file() or (FIXTURES / name).read_bytes() != data:
                raise SystemExit(f"stale fixture: {name}")
        if (FIXTURES / "manifest.json").read_bytes() != manifest_bytes:
            raise SystemExit("stale fixture: manifest.json")
        return
    FIXTURES.mkdir(parents=True, exist_ok=True)
    for name, data in generated.items():
        (FIXTURES / name).write_bytes(data)
    (FIXTURES / "manifest.json").write_bytes(manifest_bytes)


if __name__ == "__main__":
    main()

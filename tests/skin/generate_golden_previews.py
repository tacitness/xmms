#!/usr/bin/env python3

import argparse
import io
import os
import struct
import tempfile
import zipfile
import zlib


CANVAS_WIDTH = 550
CANVAS_HEIGHT = 348
BACKGROUND = (15, 18, 20, 255)
PANEL = (36, 41, 46, 255)
ASSETS = [
    ("main.bmp", 0, 0),
    ("pledit.bmp", 0, 116),
    ("eqmain.bmp", 275, 0),
    ("titlebar.bmp", 275, 116),
    ("cbuttons.bmp", 275, 144),
    ("numbers.bmp", 275, 232),
    ("volume.bmp", 383, 232),
    ("shufrep.bmp", 275, 262),
    ("posbar.bmp", 331, 262),
    ("playpaus.bmp", 275, 292),
    ("monoster.bmp", 331, 292),
    ("text.bmp", 387, 292),
]


def load_bmp(data: bytes):
    if data[:2] != b"BM":
        raise ValueError("not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    if dib_size < 40:
        raise ValueError("unsupported BMP header")
    width, height, planes, depth, compression = struct.unpack_from("<iiHHI", data, 18)
    if planes != 1 or depth != 24 or compression != 0:
        raise ValueError("only uncompressed 24-bit BMP is supported")

    top_down = height < 0
    height = abs(height)
    row_stride = ((width * 3) + 3) & ~3
    pixels = []
    for y in range(height):
        src_y = y if top_down else (height - 1 - y)
        row_off = offset + (src_y * row_stride)
        row = []
        for x in range(width):
            b, g, r = data[row_off + (x * 3): row_off + (x * 3) + 3]
            row.append((r, g, b, 255))
        pixels.append(row)
    return width, height, pixels


def write_png(path: str, width: int, height: int, pixels):
    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + tag + payload +
                struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    raw = bytearray()
    for row in pixels:
        raw.append(0)
        for r, g, b, a in row:
            raw.extend((r, g, b, a))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(chunk(b"IHDR", ihdr))
    png.extend(chunk(b"IDAT", zlib.compress(bytes(raw), level=9)))
    png.extend(chunk(b"IEND", b""))

    with open(path, "wb") as handle:
        handle.write(png)


def new_canvas():
    pixels = [[BACKGROUND for _ in range(CANVAS_WIDTH)] for _ in range(CANVAS_HEIGHT)]
    for y in range(116):
        for x in range(275):
            pixels[y][x] = PANEL
            pixels[y + 116][x] = PANEL
            pixels[y][x + 275] = PANEL
    return pixels


def blit(canvas, image, x_off: int, y_off: int):
    width, height, pixels = image
    for y in range(height):
        if y + y_off >= CANVAS_HEIGHT:
            break
        row = canvas[y + y_off]
        src = pixels[y]
        for x in range(width):
            if x + x_off >= CANVAS_WIDTH:
                break
            row[x + x_off] = src[x]


def extract_archive(archive_path: str, dest_dir: str):
    with zipfile.ZipFile(archive_path) as archive:
        for member in archive.namelist():
            name = os.path.basename(member)
            if not name:
                continue
            with archive.open(member) as source, open(os.path.join(dest_dir, name), "wb") as out:
                out.write(source.read())


def generate_preview(archive_path: str, output_path: str):
    canvas = new_canvas()
    with tempfile.TemporaryDirectory() as tmpdir:
        extract_archive(archive_path, tmpdir)
        assets = {name.lower(): name for name in os.listdir(tmpdir)}
        for asset_name, x, y in ASSETS:
            match = assets.get(asset_name.lower())
            if match is None:
                continue
            with open(os.path.join(tmpdir, match), "rb") as handle:
                image = load_bmp(handle.read())
            blit(canvas, image, x, y)
    write_png(output_path, CANVAS_WIDTH, CANVAS_HEIGHT, canvas)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixtures-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)
    for stem in ("classic", "custom-overlay", "missing-eqmain"):
        generate_preview(
            os.path.join(args.fixtures_dir, f"{stem}.wsz"),
            os.path.join(args.output_dir, f"{stem}.png"),
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

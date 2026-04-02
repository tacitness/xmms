#!/usr/bin/env python3

import argparse
import os
import struct
import zipfile


def write_bmp(path: str, width: int, height: int, seed: int) -> None:
    row_bytes = width * 3
    padding = (4 - (row_bytes % 4)) % 4
    pixel_rows = []
    for y in range(height):
        row = bytearray()
        for x in range(width):
            r = (x * 5 + seed * 31) % 256
            g = (y * 3 + seed * 47) % 256
            b = ((x + y) * 2 + seed * 59) % 256
            if ((x // 8) + (y // 8)) % 2 == 0:
                r, g, b = b, r, g
            row.extend([b, g, r])
        row.extend(b"\x00" * padding)
        pixel_rows.append(bytes(row))

    pixel_data = b"".join(reversed(pixel_rows))
    file_size = 14 + 40 + len(pixel_data)
    with open(path, "wb") as handle:
        handle.write(b"BM")
        handle.write(struct.pack("<IHHI", file_size, 0, 0, 54))
        handle.write(struct.pack("<IIIHHIIIIII", 40, width, height, 1, 24, 0,
                                 len(pixel_data), 2835, 2835, 0, 0))
        handle.write(pixel_data)


def build_skin_dir(base: str, variant: str) -> None:
    os.makedirs(base, exist_ok=True)

    assets = {
        "main.bmp": (275, 116, 1),
        "pledit.bmp": (275, 116, 2),
        "eqmain.bmp": (275, 116, 3),
        "titlebar.bmp": (275, 28, 4),
        "cbuttons.bmp": (136, 18, 5),
        "numbers.bmp": (108, 13, 6),
        "volume.bmp": (68, 13, 7),
        "shufrep.bmp": (46, 15, 8),
        "posbar.bmp": (248, 10, 9),
        "playpaus.bmp": (28, 12, 10),
        "monoster.bmp": (56, 12, 11),
        "text.bmp": (155, 6, 12),
    }

    if variant == "custom-overlay":
        assets["main.bmp"] = (275, 116, 21)
        assets["pledit.bmp"] = (275, 116, 22)
        assets["eqmain.bmp"] = (275, 116, 23)
        assets["titlebar.bmp"] = (275, 28, 24)
    elif variant == "missing-eqmain":
        assets.pop("eqmain.bmp")

    for name, (width, height, seed) in assets.items():
        write_bmp(os.path.join(base, name), width, height, seed)

    if variant == "shape-region":
        with open(os.path.join(base, "region.txt"), "w", encoding="utf-8") as handle:
            handle.write("[Normal]\n")
            handle.write("NumPoints=5\n")
            handle.write("PointList=0,0,274,0,274,115,120,115,0,40\n")


def zip_dir(source_dir: str, output_path: str, extra_entries=None) -> None:
    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for root, _, files in os.walk(source_dir):
            for filename in sorted(files):
                full_path = os.path.join(root, filename)
                rel_path = os.path.relpath(full_path, source_dir)
                archive.write(full_path, arcname=rel_path)
        if extra_entries:
            for arcname, data in extra_entries:
                archive.writestr(arcname, data)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    for variant in ("classic", "custom-overlay", "missing-eqmain", "shape-region"):
        skin_dir = os.path.join(args.output_dir, variant)
        build_skin_dir(skin_dir, variant)
        zip_dir(skin_dir, os.path.join(args.output_dir, f"{variant}.wsz"))

    malicious_dir = os.path.join(args.output_dir, "malicious-traversal")
    build_skin_dir(malicious_dir, "classic")
    zip_dir(
        malicious_dir,
        os.path.join(args.output_dir, "malicious-traversal.wsz"),
        extra_entries=[("../../escape.txt", "nope\n")],
    )

    with open(os.path.join(args.output_dir, "corrupt.wsz"), "w", encoding="utf-8") as handle:
        handle.write("this is not a valid zip archive\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

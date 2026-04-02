#!/usr/bin/env python3

import argparse
import json
import os
import shutil
import subprocess
import sys


DEFAULT_QUERY = "*.wsz"


def run_ia_download(output_dir: str) -> int:
    ia = shutil.which("ia")
    if ia is None:
        print("internetarchive CLI not found; install it with `pip install internetarchive`",
              file=sys.stderr)
        return 1

    cmd = [
        ia,
        "download",
        "winampskins",
        f"--glob={DEFAULT_QUERY}",
        "--destdir",
        output_dir,
        "--no-directories",
    ]
    return subprocess.call(cmd)


def write_manifest(output_dir: str) -> None:
    manifest = {
        "source": "https://archive.org/details/winampskins",
        "selection_notes": [
            "Use the downloaded corpus to curate one classic reference skin.",
            "Select at least one skin with custom EQ and playlist overlays.",
            "Select at least one skin with non-rectangular region.txt masks.",
            "Retain one intentionally malformed archive for robustness coverage.",
        ],
    }
    with open(os.path.join(output_dir, "manifest.json"), "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2)
        handle.write("\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)
    result = run_ia_download(args.output_dir)
    if result == 0:
        write_manifest(args.output_dir)
    return result


if __name__ == "__main__":
    raise SystemExit(main())

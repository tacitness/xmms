# Skin Compatibility Tests

This directory provides a minimal compatibility suite for Winamp 2.x style skin archives.

`generate_skin_fixtures.py` creates deterministic `.wsz` fixtures used by CI:
- `classic.wsz`: baseline archive with the core BMP assets.
- `custom-overlay.wsz`: alternate colors/layout assets for preview regression coverage.
- `missing-eqmain.wsz`: omits `eqmain.bmp` to exercise fallback handling.
- `shape-region.wsz`: includes `region.txt` for future shape-mask coverage once issue `#26` lands.
- `malicious-traversal.wsz`: contains `../../escape.txt` and must be rejected.
- `corrupt.wsz`: malformed archive for no-crash coverage.

The preview regression test renders a deterministic composite PNG from the extracted skin assets and
compares its average hash against the committed goldens in `tests/skin/golden/`.

To acquire a larger manual corpus from the Internet Archive, use:

```bash
python3 tests/skin/download_archive_org_corpus.py --output-dir tests/skin/corpus/archive_org
```

Manual smoke-test matrix:

| Fixture | Load | Main | Playlist | EQ | Missing Assets | Traversal Guard |
|---|---|---|---|---|---|---|
| `classic.wsz` | yes | yes | yes | yes | n/a | n/a |
| `custom-overlay.wsz` | yes | yes | yes | yes | n/a | n/a |
| `missing-eqmain.wsz` | yes | yes | yes | fallback | yes | n/a |
| `shape-region.wsz` | yes | yes | yes | yes | n/a | pending `#26` |
| `malicious-traversal.wsz` | reject | n/a | n/a | n/a | n/a | yes |
| `corrupt.wsz` | reject | n/a | n/a | n/a | n/a | n/a |

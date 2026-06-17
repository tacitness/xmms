# XMMS Fuzzing Harnesses

libFuzzer harnesses targeting XMMS's untrusted-input parsers. These exercise
the code paths that process attacker-influenced data: playlist files, config
files, and song-title format strings.

## Targets

| Harness | Function under test | Surface |
|---------|--------------------|---------|
| `fuzz_configfile` | `xmms_cfg_open_file` | INI / `.pls` playlist parser |
| `fuzz_titlestring` | `xmms_get_titlestring` | `%`-escape title-format parser |

## Build & run

Requires clang. Fuzzers are off by default.

```bash
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang \
    -DXMMS_GTK_VERSION=3 \
    -DXMMS_BUILD_FUZZERS=ON
cmake --build build --target fuzz_configfile fuzz_titlestring

# Short smoke (this is what CI runs):
ctest --test-dir build -R _smoke --output-on-failure

# Longer campaign:
./build/tests/fuzz/fuzz_titlestring -max_total_time=300 tests/fuzz/corpus/fuzz_titlestring
```

When `XMMS_BUILD_FUZZERS=ON` the whole project is compiled with
`-fsanitize=fuzzer-no-link,address,undefined` for coverage + memory checking;
each harness links `-fsanitize=fuzzer`.

## Corpus

`corpus/<harness>/` holds seed inputs and regression cases. Regression seeds
(prefix `regression_`) reproduce previously-fixed bugs and must stay green.

## Bugs found

| Seed | Bug | Fix |
|------|-----|-----|
| `regression_trailing_percent` | heap over-read: format string ending in `%` advanced the cursor past the NUL (`titlestring.c` `parse_variable`) | don't consume the terminating NUL |
| `regression_width_overflow` | signed-integer-overflow + DoS: unbounded `%`-field width (e.g. `%999999999p`) | clamp width/precision to `XMMS_TITLE_MAX_PAD` during parsing |

## Adding a harness

1. Add `fuzz_<name>.c` defining `LLVMFuzzerTestOneInput`.
2. Add `<name>` to `XMMS_FUZZERS` in `CMakeLists.txt`.
3. Seed `corpus/fuzz_<name>/` with a few valid inputs.

Good future targets (need a small refactor to decouple parsing from GTK/global
state first): the m3u playlist line reader and the Winamp skin loader.

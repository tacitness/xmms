fix(mpg123): 2× duration and early stop for CBR 48 kHz MP3 — "Info" tag not recognised in mpg123_get_xing_header()
## Summary

The mpg123 input plugin reports **2× the correct duration** for CBR MP3 files encoded with LAME at 48 000 Hz, and playback stops at roughly the halfway point (the real end of audio).

## Reproduction

All tracks below are MPEG-1 Layer 3, 128 kbps CBR, **48 000 Hz** stereo, encoded with LAME:

| Track | File size | Actual duration | XMMS reports |
|---|---|---|---|
| Could It Be | 4.5 MB | 4:49 | **9:39** |
| Guided | 2.6 MB | 2:48 | **5:37** |
| Motions | 3.6 MB | 3:49 | **7:39** |
| Timelines (full mix) | 19 MB | 20:03 | **40:09** |

Ratio is consistently **2.0×**. Playback audio ends after ~4:49 while the progress bar still shows ~4:49 remaining.

## Root Cause

`mpg123_get_xing_header()` in `Input/mpg123/dxhead.c` only recognises the `"Xing"` VBR tag:

```c
/* dxhead.c line 44 */
if (strncmp(buf, "Xing", 4))
    return 0;
```

LAME writes an **`"Info"` tag** for CBR files (same format, different four-byte magic). The function silently returns 0, the Info header is ignored, and `get_song_time()` falls back to the file-size ÷ bytes-per-frame estimate using the bitrate of the **null Info frame** (64 kbps) rather than the 128 kbps audio frames:

```
bpf (64 kbps, 48 kHz) = 144 × 64 000 / 48 000 = 192 bytes/frame   ← half correct
bpf (128 kbps, 48 kHz) = 384 bytes/frame                            ← correct
```

`len / 192` = 2 × `len / 384` → duration is doubled.

## Affected Files

- `Input/mpg123/dxhead.c` — `mpg123_get_xing_header()` (line 44)

## Fix

Accept both `"Xing"` (VBR) and `"Info"` (CBR) tags:

```c
if (strncmp(buf, "Xing", 4) && strncmp(buf, "Info", 4))
    return 0;
```

This is the standard fix used by every modern mpg123-based player. The `Info` and `Xing` headers are byte-for-byte identical in structure — only the magic differs.

## Additional Notes

- 44 100 Hz CBR files are likely unaffected because `mpg123_get_first_frame()` tends to skip the null Info frame for 44 100 Hz files (the frame boundary arithmetic lands differently).
- The secondary fallback issue (using the null frame's bitrate instead of scanning ahead for the real audio bitrate) is masked once the Info header is properly detected.
- `mpg123_stream_check_for_xing_header()` in `common.c` also calls `mpg123_get_xing_header()` and is therefore equally affected during the live-playback length calculation path.

## Test Files

```
/mnt/ascender/data/Music/Chillstep/Attom/Timelines/
```
All five tracks exhibit the bug. "Forever" (3:43) coincidentally reports correctly because `mpg123_get_first_frame()` skips the 64 kbps null frame for that specific file, landing on the true 128 kbps frames for the size-based estimate.

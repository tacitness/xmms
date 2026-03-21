bug(eq): EQ causes audio clipping/distortion at high band gain — preamp compensation not applied
## Description
When one or more EQ bands are boosted significantly (especially with the preamp
at or near 0 dB), the output audio clips and distorts audibly. This was a
known issue in XMMS 1.x and is unresolved in the current GTK3 port.

## Root Cause
XMMS applies EQ by multiplying PCM sample values by a computed gain factor per
band. When multiple bands are boosted simultaneously the sum can exceed the
maximum sample value, causing integer overflow / clipping. The XMMS preamp
amplifies this further.

Classic mitigation: **headroom-aware preamp auto-compensation** — lower the
preamp gain automatically in proportion to the total positive band gain so the
output never exceeds 0 dBFS. XMMS never implemented this.

## Repro
1. Enable the Equalizer.
2. Boost all 10 bands to +12 dB and set preamp to 0 dB.
3. Play any audio at normal volume.
4. Hear distortion / clipping even on quiet source material.

## Potential Fix
- Compute total excess gain and subtract it from the preamp before passing
  bands to the input plugin (`input_set_eq()`).
- Alternatively add soft-clipping / limiter in the audio processing path
  (`Input/mpg123/` or a new Effect plugin).
- Add a user-visible warning when total gain exceeds 0 dBFS.

## References
- Original XMMS bug tracker: known historical issue, no upstream fix shipped.
- Related: XMMS2, Audacious, BMP all addressed this with headroom compensation.

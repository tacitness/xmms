bug(playlist): bottom button bar renders black overflow block beyond button edges
## Summary

The bottom control bar of the playlist window renders with a **large black rectangle** that extends beyond the visible skin button area, covering several pixels beyond the painted button edges. The region between the last button and the right edge of the playlist, and below the buttons, is filled with opaque black instead of being transparent or matching the skin background.

## Visual

Screen captures show the black overflow clearly against the skin background — the playlist window bottom-right corner has a plain black block where the skin texture should continue.

## Root Cause

The playlist window bottom-section is drawn using `playlistwin_gc` (a `GdkGC`/cairo equivalent), but the drawing area for the button row is not properly clipped or masked to the actual button extents.  The background fill is applied to the full row height × full playlist width rather than only behind the button sprites.  

This is likely related to the `GdkPixmap` → cairo migration: the old `GdkPixmap`-based code used a shaped/masked pixmap that confined drawing to the button regions, but the current cairo path fills the entire drawing rectangle.

## Steps to Reproduce

1. Open the playlist window  
2. Observe the bottom button row (ADD/REM/SEL/MISC/…)  
3. Black overfill is visible beyond the rightmost button

## Affected Files

- `xmms/playlistwin.c` — `playlistwin_draw_frame()` / bottom button background paint
- `xmms/svis.c` / `xmms/draw.c` — drawing context for playlist extras

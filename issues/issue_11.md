bug(eq): EQ band and preamp sliders lag — equalizerwin_motion calls draw_main_window instead of draw_equalizer_window
## Description
During EQ slider drag, `equalizerwin_motion()` in `xmms/equalizer.c` calls
`draw_main_window(FALSE)` in the non-moving branch instead of
`draw_equalizer_window(FALSE)`. The EQ window does not repaint itself in
response to its own motion events; it only repaints on the 10 ms idle-func
tick, introducing a perceptible refresh delay when dragging any EQ slider.

## Steps to Reproduce
1. Open the Equalizer window.
2. Click and drag any band slider or the preamp slider quickly.
3. Observe the slider knob position lags noticeably behind the cursor.

## Root Cause
```c
// xmms/equalizer.c — equalizerwin_motion()
} else {
    handle_motion_cb(equalizerwin_wlist, widget, event);
    draw_main_window(FALSE);   // BUG: should be draw_equalizer_window(FALSE)
}
```

## Expected Behaviour
Slider knobs track cursor in real-time (same behaviour as mainwin sliders).

## Fix
Replace `draw_main_window(FALSE)` with `draw_equalizer_window(FALSE)`
inside `equalizerwin_motion()`.

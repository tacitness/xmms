bug(output): Volume and balance controls have no effect — ALSA mixer device defaults to 'PCM' which does not exist on most modern systems
## Description
Moving the volume or balance slider in the main window or equalizer window
updates the visual and the info-bar text but does not change the audio level.
The actual system volume is unaffected.

## Root Cause
`Output/alsa/init.c` initialises the mixer device name to `"PCM"`:
```c
alsa_cfg.mixer_device = g_strdup("PCM");
```
On modern ALSA setups (PulseAudio/PipeWire dummy-ALSA, HAD Intel, USB audio,
etc.) the playback mixer element is named `"Master"`, not `"PCM"`.
`alsa_setup_mixer()` calls `alsa_get_mixer_elem(mixer, "PCM", 0)` which
returns `NULL`; consequently `pcm_element = NULL` and both
`alsa_get_volume()` and `alsa_set_volume()` return immediately without
touching hardware.

Additionally `alsa_set_volume()` never calls `alsa_setup_mixer()` on its
own, so the mixer may be uninitialized on first volume-set.

## Confirmed via
```
amixer                 # shows: Master, Headphone — no 'PCM' element
```

## Fix
1. Change default in `Output/alsa/init.c`:
   `alsa_cfg.mixer_device = g_strdup("Master");`
2. Add mixer fallback list in `alsa_setup_mixer()` (try Master → PCM → first element).
3. Call `alsa_setup_mixer()` from `alsa_set_volume()` when `mixer_start == TRUE`.

bug(alsa): playlist does not auto-advance to next track on PipeWire — alsa_playing() never returns FALSE
## Summary

After a track ends, XMMS does not advance to the next song in the playlist. Playback stops silently and the progress bar freezes. The "Repeat" and shuffle flags have no effect.

## Expected Behaviour

When a track ends:
- If there are more tracks in the list → play the next track automatically  
- If on the last track and Repeat is ON → wrap to the first track
- If on the last track and Repeat is OFF → stop (stop is correct, but no crash/freeze)

## Root Cause

`playlist_eof_reached()` is only called from `idle_func()` when `input_get_time()` returns `-1`.  
`get_time()` in the mpg123 plugin returns `-1` only when:

```c
mpg123_info->eof && !mpg123_ip.output->buffer_playing()
```

`buffer_playing()` maps to `alsa_playing()` in `Output/alsa/audio.c`:

```c
int alsa_playing(void)
{
    if (!going || paused || alsa_pcm == NULL)
        return FALSE;
    return snd_pcm_state(alsa_pcm) == SND_PCM_STATE_RUNNING;
}
```

On **PipeWire** (via its ALSA compatibility layer) and some `dmix` configurations, the PCM **never leaves `SND_PCM_STATE_RUNNING`** after the last samples play — instead of triggering an underrun (`XRUN`) it silently outputs silence. `alsa_playing()` therefore always returns `TRUE`, `get_time()` never returns `-1`, and `playlist_eof_reached()` is never called.

## Secondary Issue (same PR)

`alsa_get_output_time()` casts the (signed) value from `snd_pcm_delay()` to `unsigned int` without checking for negative values. On PipeWire, `snd_pcm_delay()` can return a negative frame count once the hw cursor passes the last write point, producing a very large `unsigned int` that incorrectly zeros the output time.

## Fix

`alsa_playing()`: when the ALSA PCM is still `RUNNING` but our intermediate ring buffer is empty, additionally check `snd_pcm_delay()`. If delay ≤ 0, our data has fully played.

`alsa_get_output_time()`: add `delay > 0` guard before using the delay value.

### Affected Files

- `Output/alsa/audio.c` — `alsa_playing()` and `alsa_get_output_time()`

## Environment

- PipeWire 1.2.7 (ALSA passthrough / pipewire-alsa)  
- ALSA lib 1.2.13  
- Kernel: Linux 6.x

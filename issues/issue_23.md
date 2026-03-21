feat(vis): next-gen visualization plugin suite — 3D spatial audio, particle systems, xscreensaver/xlockmore integration
## Overview

Following the OpenGL Spectrum Analyzer GL 3.3 port, this issue tracks a new suite of advanced visualization plugins covering modern 3D rendering techniques, audio-reactive particle systems, spatial audio representations, and xlockmore/xscreensaver beat-reactive lockscreen integration.

All plugins: **GL 3.3 core + GtkGLArea + GLib**. No X11 direct. No threads.

---

## Plugin Backlog

### 1. `waveform_tunnel` — 3D Waveform Tunnel / Starfield
**Inspiration:** classic Winamp Milkdrop tunnel, xscreensaver `starwars`/`tunnel`

Audio-reactive tunnel rendered with GL_TRIANGLE_STRIP rings. Each ring's radius is modulated by PCM waveform amplitude. The tunnel recedes into the screen over time creating a fly-through effect. Bass frequencies pulse the tunnel diameter. Color cycles through HSV space mapped to frequency centroid.

- Input: PCM waveform `render_pcm`
- Geometry: N rings × M segments TRIANGLE_STRIP, Z-extruded over time
- Controls: arrow keys adjust speed, space bar toggles color mode

---

### 2. `particle_storm` — Audio-Reactive 3D Particle System
**Inspiration:** Milkdrop presets, Three.js particle visualizers

10,000–50,000 particles on GPU (point sprites or instanced quads). Each particle has position, velocity, lifetime. Beat detection triggers particle explosions at random positions. Frequency band energies map to particle velocity components (bass = outward burst, treble = upward drift). Particles fade out over ~2s. Point size proportional to amplitude.

- GL: `GL_POINTS` or instanced `GL_TRIANGLES`; transform feedback for GPU-side update, or CPU update with double-buffered VBO
- Shader: point sprite alpha fade, additive blending (`glBlendFunc(GL_SRC_ALPHA, GL_ONE)`)
- Beat detection: simple energy-over-average threshold on bass band

---

### 3. `spectrograph_waterfall` — 2D/3D Scrolling Spectrograph
**Inspiration:** SDR waterfall displays, audio forensics tools, xoscope

Classic spectrograph: frequency on X, time on Y (or Z in 3D mode), power mapped to colour (black→blue→cyan→green→yellow→red). In 3D mode the spectrograph tiles become a receding 3D plane (like looking at a hillside). A second mode renders it as a 3D height-field mesh where amplitude = Y height (like the OpenGL Spectrum but continuous, scrolling, 256-band resolution).

- 2D mode: single `GL_TEXTURE_2D` updated via `glTexSubImage2D` each frame (fast, no VBO rebuild)
- 3D mode: 256×N height-field, rendered as `GL_TRIANGLE_STRIP` rows
- Color map: GLSL fragment shader LUT (plasma/magma/viridis palettes)

---

### 4. `lissajous_sphere` — 3D Lissajous / Oscilloscope Sphere
**Inspiration:** analog oscilloscopes in XY mode, vectorscope displays

Left channel → X axis, right channel → Y axis, time → Z axis forming a 3D Lissajous figure that rotates slowly. With mono, a Lissajous circle. With stereo separation the figure opens into complex 3D knots. 4096 sample history rendered as a `GL_LINE_STRIP` with per-vertex alpha fade. Sidechain envelope causes the line to g

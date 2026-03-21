gtk3(output): replace OSS/ESD output plugins with PulseAudio and PipeWire
OSS and ESD (esound) are obsolete Linux audio APIs. Modern systems use PulseAudio and/or PipeWire.

Status:
- ESD: DISABLED in current build (--disable-esd). ESD libraries no longer in Ubuntu 24.04.
- OSS: Still builds but /dev/dsp not present on most modern systems.
- ALSA: Working (enabled by default).
- PulseAudio: Plugin exists (Output/pulse/) but needs updating for libpulse 16.x API.
- PipeWire: New plugin needed — PipeWire provides full PulseAudio compatibility layer.

Tasks:
- [ ] Update Output/alsa/ for ALSA 1.2.x API
- [ ] Update Output/esd/ or remove entirely
- [ ] Create Output/pulse/ with modern libpulse API
- [ ] Create Output/pipewire/ using native PipeWire API

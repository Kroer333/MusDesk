# Changelog

## Unreleased

- FFT spectrum ring with 32 logarithmic stereo power bands, cached during import;
  preset v3 preserves older v1/v2 appearances while adding spectrum intensity.
- Five aspect ratios (9:16, 16:9, 1:1, 3:4, 4:3), each at a 720 or 1080 pixel
  short edge, with dedicated layouts and matching preview/MP4 dimensions.
- Project v2 persists aspect ratio and resolution; v1 projects keep 1080 × 1920.

- Beat-gated vinyl/text scaling, bass-reactive background motion and animated rim
  waves, shared by preview and MP4 export with individual reaction controls.
- Preset version 2 stores beat/reaction settings; version 1 presets and embedded
  project presets migrate with new effects disabled to preserve their old appearance.

- Bass-band energy and onset analysis during import, with deterministic beat-pulse
  preview meters and onset markers on the waveform.

- Peak-preserving audio waveform in the preview controls, with playhead,
  click/drag seeking and bounded background analysis of decoded stereo audio.

- Collect project into a new portable folder with copied audio/artwork and relative
  paths, background copying, cancellation and protection of existing files.

- Recovery copies every 30 seconds for unsaved compositions, stored separately
  from manually saved projects and isolated between running instances.
- Inactive-session recovery, deferred restore and confirmed removal of old copies;
  restored sessions require an explicit Save As.

- New/Open/Save/Save As for `.musdesk` JSON projects, including labels, visual
  settings, relative media paths and paused restoration of the preview position.
- Missing-media relinking, project shortcuts and unsaved-change confirmation.
- Safe project writes, strict version/size/text validation and source-media protection.
- Opening/new projects clear old media rather than reusing an unrelated composition.

## 0.1.0

- SDL3/OpenGL desktop editor and a vertical 1080 × 1920 composition.
- PNG/JPEG/still WebP artwork; rotating vinyl, track labels and moving background.
- MP3/WAV/OGG import, playback/seek controls and deterministic RMS-reactive glow.
- Soft Glow / Beat Pulse and validated JSON preset persistence.
- 30 FPS H.264/AAC MP4 export with hardware trials, CPU fallback, progress,
  cancellation, ffprobe verification and preservation of existing output on failure.
- Source-audio change detection, export readiness messages and destination checks.
- Version reporting, dependency notices and a Release-only Windows x64 ZIP.
- MIT license for MusDesk's own source code; public packages omit external FFmpeg tools.
- Application icon and Windows executable version metadata.
- Visual seeking and MP4 export remain available when audio output fails;
  the editor can retry the output device without reimporting the clip.
- Cancellable FFmpeg/ffprobe detection, including a running version probe.
- English-only application interface; no language selector or translations yet.
- Personal private package with FFmpeg/ffprobe, GPLv3 notices, build information
  and tool hashes; the tools-free package remains available separately.

Known limits: no project saving, no animated WebP or JPEG EXIF orientation,
20-minute in-memory audio limit, possible hardware output latency, and varying
encoder speed/compression. NVIDIA/AMD hardware and a clean Windows installation
have not been independently verified. FFmpeg/ffprobe are included in the personal
private ZIP and must be installed separately for the tools-free ZIP.
See `RELEASE_NOTES.md` for first-run instructions.

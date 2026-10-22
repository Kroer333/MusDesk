# Changelog

## 0.1.0 — local evaluation

- SDL3/OpenGL desktop editor and a vertical 1080 × 1920 composition.
- PNG/JPEG/still WebP artwork; rotating vinyl, track labels and moving background.
- MP3/WAV/OGG import, playback/seek controls and deterministic RMS-reactive glow.
- Soft Glow / Beat Pulse and validated JSON preset persistence.
- 30 FPS H.264/AAC MP4 export with hardware trials, CPU fallback, progress,
  cancellation, ffprobe verification and preservation of existing output on failure.
- Source-audio change detection, export readiness messages and destination checks.
- Version reporting, dependency notices and a Release-only local evaluation ZIP.

Known limits: no project saving, no animated WebP or JPEG EXIF orientation,
20-minute in-memory audio limit, possible hardware output latency, and varying
encoder speed/compression. NVIDIA/AMD hardware and a clean Windows installation
have not been independently verified. No public release or source license selected.

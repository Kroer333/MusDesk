# Changelog

## Unreleased

- Added proportional corner resizing, center/safe-margin snapping, editor guides and Alt bypass.
- Added preview selection and dragging for foreground and labels/timeline, with grouped Undo/Redo and Escape cancellation.
- Fixed hidden/moved captions restricting spectrum deformation and outdated FFmpeg restart guidance.

## 0.3.0 — pre-release

- Added FFmpeg setup guidance and detection retry without restarting the app.
- Made the documented public release workflow tools-free and clarified FFmpeg setup.
- Fixed hidden labels/timeline limiting vertical placement and overlay pulse; kept progress markers inside the frame.
- Added Vinyl, Artwork card, Spectrum bars and Background & titles scenes.
- Added artwork/gradient/solid backgrounds, element visibility, placement and size controls.
- Expanded independent Glow/Spectrum controls, smoothing and resets.
- Added grouped Undo/Redo and selected-range MP4 export.
- Project v4 and preset v7 read older formats; new saves require MusDesk 0.3.0.

## 0.2.0 — 2026-10-31

- Added project saving, missing-media relinking, autosave/recovery and portable collection.
- Added waveform seeking and five output ratios at 720/1080 px short edge.
- Added independent artwork framing and background softness.
- Added FFT spectrum, separate effect switches and the Side Kick/Spectrum Pulse presets.
- Improved attack detection with frequency bands and a shared dBFS gate.
- Improved spectrum shape, preview sizing and workspace navigation.
- Project v3 / preset v6 support migration from earlier versions.

## 0.1.0

- Initial SDL3/OpenGL editor: rotating vinyl, artwork, labels and audio-reactive glow.
- Audio/image import, preview playback, JSON presets and vertical 1080 × 1920 output.
- H.264/AAC export with hardware trials, CPU fallback, progress and cancellation.
- Windows x64 ZIP, application icon, MIT license and dependency notices.

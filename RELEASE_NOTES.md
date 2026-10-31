# MusDesk 0.2.0

A major feature update after 0.1.0 for Windows x64: save compositions, choose
portrait/landscape/square output and build audio-reactive music videos.
**The application interface currently supports English only.**

## Download and setup

- Download **MusDesk-0.2.0-private-windows-x64.zip** and extract the whole folder.
- Windows 10/11 x64 and an OpenGL 3.3 graphics driver are required.
- FFmpeg and ffprobe are included in the personal private package. Keep the
  assets, presets, licenses and DLLs beside MusDesk.exe; Visual Studio is not required.
- The optional tools-free **MusDesk-0.2.0-windows-x64.zip** needs independently
  supplied FFmpeg and ffprobe, including libx264/AAC support for CPU export.
- The `.zip.sha256` attachment verifies the archive. GitHub's automatic source
  archives contain source code, not the runnable application.

## New since 0.1.0

- New/Open/Save/Save As for versioned `.musdesk` projects, relative media paths,
  missing-media relinking and protection against losing unsaved changes.
- Per-instance autosave/recovery and Collect project for portable media copies.
- Audio waveform with click/drag seeking and shared audio/visual preview timing.
- Five output formats: 9:16, 16:9, 1:1, 3:4 and 4:3; a 720 or 1080 pixel short
  edge at 30 FPS. Export retains full selected resolution independently of preview.
- Independent artwork/background framing and background softness.
- Separate glow, FFT outline, rim waves, overlay pulse, background motion and
  Side Kick effects; four included presets: Soft Glow, Beat Pulse, Side Kick
  and Spectrum Pulse.
- Filtered attack detection with a shared dBFS hit gate for overlay pulse and
  Side Kick, selectable Bass/Drums/Wide bands and improved rapid-hit handling.
- 32-band FFT, display gain/balance and an asymmetric 128-vertex spectrum outline
  with angular peaks and timestamp-based movement.
- Separate editor sections and preview resolution matched to its screen size.

## Projects and presets

- Project v3 reads project v1/v2; preset v6 reads presets v1-v5.
- Keep backups of older projects. MusDesk 0.1.0 cannot read the newer schemas.
- Old confidence thresholds are starting values for the new dBFS gate and may
  need readjustment. Older named glow presets migrate with line effects disabled.
- A normal project references audio/artwork; it does not embed them. Use
  **Collect project** when moving it to another computer or folder.
- Exported MP4 files are independent videos; they do not contain an editable project.

## Known limitations and checks

- Windows x64 only; English UI only. No batch export or JPEG EXIF orientation;
  animated WebP is unsupported.
- Audio stays in memory, limited to 20 minutes; decoding has a 120-second timeout.
- Band-based attack detection does not isolate individual instruments. Spectrum
  detail includes procedural movement driven by spectral energy.
- Preview device buffering and graphics load can affect perceived synchronization.
- CPU/Intel QSV have been exercised locally. NVIDIA NVENC, AMD AMF and a clean
  Windows installation still need physical coverage.
- Automated tests and full video/audio decoding do not replace external-player
  listening or subjective assessment of beat selection and synchronization.

## Personal private package

The bundled-tools archive retains the personal private scope of 0.1.0. Read
`PRIVATE_PACKAGE.txt` and the bundled dependency notices before sharing copies.
MusDesk's own code is MIT licensed; bundled dependencies retain their licenses.
This is a pre-release while remaining acceptance checks are open.

Report issues with your Windows version, graphics device/driver, selected encoder
and exact error. Do not attach private audio or artwork to issue reports.

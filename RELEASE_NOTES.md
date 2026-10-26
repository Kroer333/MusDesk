# MusDesk 0.1.0

First personal private Windows x64 release of MusDesk: turn artwork and a music
track into an animated vertical video. **The application interface currently
supports English only; other interface languages are not available yet.**

## Download and setup

- Download **MusDesk-0.1.0-private-windows-x64.zip**, then extract the whole folder.
- Requires Windows 10/11 x64 and a graphics driver supporting OpenGL 3.3.
- **FFmpeg and ffprobe are included** beside MusDesk.exe; no separate media-tool
  installation is needed for this package. Keep the assets, licenses and DLLs together.
- The alternative tools-free **MusDesk-0.1.0-windows-x64.zip** requires a separate
  Windows x64 build using the links on the
  [official FFmpeg download page](https://ffmpeg.org/download.html), extract it,
  and copy `ffmpeg.exe` and `ffprobe.exe` from its `bin` folder next to
  `MusDesk.exe`. The build needs `libx264` and AAC support for CPU export.
- Start or restart MusDesk and check that both media tools report **ready**.
- The `.zip.sha256` attachment contains the archive's SHA-256 checksum.
  GitHub's automatic **Source code** downloads contain source files, not the
  compiled application. Choose the named Windows ZIP to run MusDesk.

## Private release scope

This full package is prepared for the owner's personal use/backup in a private
repository. A release does not make a private repository public; only people
with repository read access can view it. Confirm repository visibility before
uploading. Bundled tools retain GPLv3; see `PRIVATE_PACKAGE.txt` for the scope
and the additional requirements before conveying the tools to other people.

## Included

- SDL3/OpenGL editor, application icon and a 9:16 composition preview.
- PNG/JPEG/still WebP artwork, rotating vinyl and a moving background.
- MP3/WAV/OGG import, playback, seeking, volume, mute and looping in the editor.
- Deterministic RMS-reactive glow; Soft Glow / Beat Pulse presets and JSON settings.
- H.264/AAC MP4 export at 1080 × 1920 and 30 FPS, with progress, cancellation,
  output verification and protection of an existing destination on failure.
- Automatic hardware encoder trials and CPU export mode. If hardware fails
  during encoding, select CPU and retry.
- Audio output failure does not prevent visual seeking or video export.
- MIT license for MusDesk's own source code and notices for bundled dependencies.

## Known limitations

- One vertical layout; no project saving, batch export, waveform or FFT display.
- Audio is held in memory, limited to 20 minutes; decoding has a 120-second timeout.
- Animated WebP and JPEG EXIF orientation are unsupported.
- Preview-device buffering can introduce latency. Encoder speed and quality vary.
- Intel QSV and CPU export have been exercised locally. NVIDIA NVENC, AMD AMF
  and a clean Windows installation have not been independently verified.
- Full-track export and complete decoding have been checked. External-player
  listening and subjective audio/visual synchronization remain manual checks.

Please report issues with your Windows version, graphics device/driver, selected
encoder and the exact error. Do not include private tracks or artwork in reports.

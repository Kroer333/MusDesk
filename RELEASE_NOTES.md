# MusDesk 0.2.0

Windows x64 pre-release. **English UI only.**

## Install

Download `MusDesk-0.2.0-private-windows-x64.zip`, extract the entire folder and run
`MusDesk.exe`. Requires Windows 10/11 x64 and OpenGL 3.3.
FFmpeg/ffprobe are included; keep assets, presets, licenses and DLLs together.
The optional tools-free ZIP needs separate FFmpeg/ffprobe with libx264/AAC support.
The `.sha256` file verifies the ZIP; GitHub's source archives are not runnable builds.

## New since 0.1.0

- Project save/open, relative media paths, relinking, autosave/recovery and Collect project.
- Waveform seeking; independent artwork framing and background softness.
- Five aspect ratios: 9:16, 16:9, 1:1, 3:4 and 4:3; 720/1080 px short edge at 30 FPS.
- Separate glow, spectrum, rim waves, pulse, background and Side Kick effects.
- Band-selectable attack detection, dBFS gate and an asymmetric FFT spectrum outline.

## Compatibility and limits

Project v3 reads v1/v2; preset v6 reads v1–v5. Keep backups: MusDesk 0.1.0 cannot
read the new formats. Old beat thresholds may need adjustment.
Projects reference media; use **Collect project** for portability.

Audio is held in memory (20-minute limit; 120-second decode timeout).
No animated WebP, JPEG EXIF orientation or batch export.
Detection does not isolate instruments; device buffering/render load can affect sync.
CPU/Intel QSV were checked locally; NVIDIA/AMD, clean Windows installation and
external-player listening/sync checks remain incomplete.

Own code: MIT; dependencies retain their licenses. The bundled-tools ZIP is for
personal private use; see `PRIVATE_PACKAGE.txt` and [dependency notices](THIRD_PARTY_NOTICES.md).
Report issues with the exact error, Windows/GPU/driver and encoder; omit private media.

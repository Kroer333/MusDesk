# MusDesk 0.3.0

Public Windows x64 pre-release. **English UI only.**

## Install

Download `MusDesk-0.3.0-windows-x64.zip`, extract the entire folder and run
`MusDesk.exe`. Requires Windows 10/11 x64 and OpenGL 3.3; keep DLLs and assets together.

**FFmpeg and ffprobe are not included.** Obtain a Windows build through the
[official download page](https://ffmpeg.org/download.html), extract it, and copy
`ffmpeg.exe` and `ffprobe.exe` beside `MusDesk.exe`. CPU export needs libx264 and AAC.
In **Media and preview audio**, select **Check tools again** after adding them.
The `.zip.sha256` attachment verifies the ZIP. GitHub's source archives are not runnable builds.

## New since 0.2.0

- Four scenes: Vinyl, Artwork card, Spectrum bars and Background & titles.
- Artwork, gradient or solid backgrounds; element visibility, size and placement controls.
- Independent Glow/Spectrum styling, extra attack/release smoothing and separate resets.
- Grouped Undo/Redo with Ctrl+Z, Ctrl+Y and Ctrl+Shift+Z.
- Selected-range MP4 export, including a quick five-second sample.
- Corrected placement/pulse limits for hidden elements and progress-marker margins.
- Clearer tool setup and detection retry without restarting the app.

## Compatibility and limits

Project v4 reads v1–v3; preset v7 reads v1–v6. **Back up older projects: MusDesk
0.2.0 cannot open newly saved files.** Projects reference media; use **Collect project**
to move them together. History and export selection are session-only.

Export remains H.264/AAC, 30 FPS, five aspect ratios with a 720/1080 px short edge.
Audio stays in memory (20-minute limit; 120-second decode timeout).
No batch export, animated WebP or JPEG EXIF orientation. Beat detection does not
isolate instruments. CPU/Intel QSV were checked locally; NVIDIA/AMD, clean Windows
and external-player listening/sync acceptance remain incomplete.

Own code: [MIT](LICENSE); see [dependency notices](THIRD_PARTY_NOTICES.md).
Report the exact error, app/Windows version, GPU/driver and encoder; omit private media.

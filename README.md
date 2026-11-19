# MusDesk

Turn music and artwork into animated MP4 videos. Windows 10/11 x64, OpenGL 3.3.
**0.3.0 pre-release · English UI only.**

![MusDesk workspace (0.1.0)](screenshots/workspace.png)

## Get started

1. Download the application ZIP from [Releases](https://github.com/Kroer333/MusDesk/releases).
2. Extract the whole folder and run `MusDesk.exe`; keep its DLLs and assets together.
3. Install FFmpeg/ffprobe as described below, then select **Check tools again** in Media.
4. Import audio and artwork, adjust the composition, then choose **Export MP4**.

Public packages omit FFmpeg/ffprobe. Obtain a Windows build through the
[FFmpeg download page](https://ffmpeg.org/download.html) and place both executables
beside `MusDesk.exe`. CPU export needs libx264 and AAC support.

## Features

- MP3/WAV/OGG audio and PNG/JPEG/still WebP artwork.
- Audio-reactive glow, spectrum, pulse and Side Kick; editable JSON presets.
- Projects, autosave/recovery, portable media collection and waveform seeking.
- H.264/AAC MP4 at 30 FPS: 9:16, 16:9, 1:1, 3:4 or 4:3; 720/1080 px short edge.

**New in 0.3.0:** Vinyl, Artwork card, Spectrum bars and Background &
titles scenes; gradient/solid backgrounds, placement/visibility controls, expanded
Glow/Spectrum settings, Undo/Redo and selected-range export.

**In development:** drag elements or resize them uniformly with corner handles.
Snap aligns to center axes/10% margins; Alt bypasses it. Guides are editor-only.
Editing keeps audio and animation playing; Esc cancels and Undo/Redo handles each gesture.
Click an element for arrow-key positioning (1 output pixel, Shift: 10; ignores Snap).
Center X/Y and reset buttons affect only the selected group.
Lock foreground/labels protects position and size for the session; effects stay editable.

Projects reference media rather than embedding it: use **Collect project** to move
everything together. Version 0.3.0 saves use project v4 / preset v7 and read older
versions; MusDesk 0.2.0 cannot open these new saves. Keep backups.

## Build

Use a Visual Studio 2022 Developer PowerShell with the C++ desktop workload,
Windows SDK and CMake 3.21+. First configuration downloads dependencies.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 1 -- /nr:false
ctest --test-dir build -C Release --output-on-failure
.\build\Release\MusDesk.exe
```

For debugging, replace `Release` with `Debug`. CMake generates build files;
the build command creates the EXE. A custom runtime output directory changes its path.
Place FFmpeg/ffprobe in `tools/ffmpeg/bin` before configuring to copy them beside
the EXE in either configuration. Runtime alternatives: `MUSDESK_FFMPEG_DIR` or `PATH`.
Run `MusDesk.exe --help` for startup options.

## Limits and links

Audio stays in memory (20-minute limit; 120-second decode timeout). Animated WebP
and JPEG EXIF orientation are unsupported. Beat detection does not isolate instruments.
Clean-Windows and NVIDIA/AMD encoder verification remain incomplete.

[Changelog](CHANGELOG.md) · [0.3.0 release notes](RELEASE_NOTES.md) ·
[Release workflow](https://github.com/Kroer333/MusDesk/blob/main/RELEASING.md) ·
[MIT license](LICENSE) · [Dependency notices](THIRD_PARTY_NOTICES.md)

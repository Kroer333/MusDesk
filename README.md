# MusDesk

**MusDesk** is a planned desktop tool for turning a music track and cover artwork into a vertical, animated music visualizer video for short-form platforms.

> **Project status: first application window.** MusDesk opens a 1280 × 800 SDL3 window and handles closing and startup errors. Automated tests cover time formatting. Editor controls, media support, audio-reactive effects, and MP4 export are not implemented yet.

## What the app will do

1. Import a music track (MP3, WAV, or OGG) and cover artwork (PNG, JPEG, or WebP).
2. Display a spinning vinyl record with centered artwork, a blurred image background, track title, artist, playback status, and progress timer.
3. Animate a glow/ring using the track's measured energy and allow choosing effect presets.
4. Preview the composition and export a synchronized, vertical MP4 video.

### First target (MVP)

- One vertical format: **1080 × 1920**, **30 FPS**, **9:16**.
- One layout, including a rotating vinyl record and audio-reactive glow.
- Basic controls for rotation speed, glow strength, and background motion.
- Preset configuration stored as JSON.
- H.264 video with AAC audio in an MP4 container.
- Consistent preview and export, with visuals calculated from an explicit playback timestamp.

This is **not** a general-purpose video editor.

## Build and run on Windows

Requirements: Visual Studio 2022 with the **Desktop development with C++** workload,
a Windows SDK, and CMake 3.21 or newer. Run these commands from the repository root
in a Developer PowerShell for Visual Studio:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --parallel 1 -- /nr:false
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\MusDesk.exe
```

Alternatively, open the repository with **File > Open > Folder** in Visual Studio,
build the Debug configuration, and select `MusDesk.exe` as the startup target.

CMake downloads the pinned SDL **3.4.18** source archive from the official SDL
GitHub release and verifies its SHA-256 checksum. The first configuration needs
internet access; sources and build products stay inside the build directory.
SDL is built from source and its DLL is copied next to `MusDesk.exe` on Windows.
Keep the DLL alongside the executable when running it elsewhere.

The current window has no rendered content. Its requested client size is 1280 × 800;
desktop scaling and window-manager constraints can affect its physical size.
Close it with the title-bar close button or **Alt+F4**. The event loop waits while
idle. The console remains available for diagnostic messages; startup failures
produce a nonzero exit code.

### MSVC debug-information troubleshooting

If building SDL reports **C1902** (program database manager mismatch), a local
workaround is to store compiler debug information in object files using `/Z7`:

```powershell
cmake -S . -B build "-DCMAKE_C_FLAGS_DEBUG=/Z7 /Ob0 /Od /RTC1" "-DCMAKE_CXX_FLAGS_DEBUG=/Z7 /Ob0 /Od /RTC1"
cmake --build build --config Debug --parallel 1 -- /nr:false
```

This changes only the Debug settings in that build directory's CMake cache and
retains debug symbols. It does not repair the toolchain or change other build
directories. See Microsoft's documentation for
[C1902](https://learn.microsoft.com/en-us/cpp/error-messages/compiler-errors-1/fatal-error-c1902)
and [debug information formats](https://learn.microsoft.com/en-us/cpp/build/reference/z7-zi-zi-debug-information-format).

## License and media

SDL is distributed under the [zlib license](https://github.com/libsdl-org/SDL/blob/release-3.4.18/LICENSE.txt).

A source-code license has not been selected. Before distributing compiled releases, review the licenses and distribution conditions of the chosen dependencies and codecs. Only use media you have permission to distribute.

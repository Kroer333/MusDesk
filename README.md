# MusDesk

**MusDesk** is a planned desktop tool for turning a music track and cover artwork into a vertical, animated music visualizer video for short-form platforms.

> **Project status: editor shell.** MusDesk opens a resizable 1280 × 800 SDL3/OpenGL window with Dear ImGui controls and an empty vertical preview. Preview color, scale, safe-area guides, and reset work. Media import, audio playback, visualizer rendering, and MP4 export are not implemented yet. Automated tests cover time formatting.

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
a Windows SDK, CMake 3.21 or newer, and a graphics driver supporting **OpenGL 3.3 Core**.
Run these commands from the repository root
in a Developer PowerShell for Visual Studio:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --parallel 1 -- /nr:false
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\MusDesk.exe
```

Alternatively, open the repository with **File > Open > Folder** in Visual Studio,
build the Debug configuration, and select `MusDesk.exe` as the startup target.

CMake downloads pinned SDL **3.4.18** and Dear ImGui **1.93.0** source archives
from their official GitHub repositories and verifies their SHA-256 checksums.
The first configuration needs internet access; sources and build products stay
inside the build directory. Both dependencies are built from source. SDL's DLL
is copied next to `MusDesk.exe` on Windows; Dear ImGui is linked statically.
Keep the DLL alongside the executable when running it elsewhere.

The requested client size is 1280 × 800, with a minimum of 900 × 600. Desktop
scaling and window-manager constraints can affect physical size. Close the window
with the title-bar close button or **Alt+F4**. Rendering uses VSync when available,
a frame delay otherwise, and pauses while minimized. Diagnostic messages go to
the console; startup failures produce a nonzero exit code.

## Current workspace

- The left panel contains preview controls and disabled media/export actions.
- The right panel fits an empty **9:16** canvas into the available space when the
  window is resized. The preview scale changes its display size, not the target
  1080 × 1920 format.
- Use the color picker or quick colors, adjust preview scale, toggle safe-area
  guides, or press **Reset preview**. These settings last only for the session.
- Keyboard navigation is enabled: **Tab**, arrow keys, and **Space** can operate
  the controls. The controls panel scrolls when space is limited.

Window/context lifetime and the rendering loop are in `src/main.cpp`. The panel,
preview placeholder, and their session state are in `src/app/EditorUi.cpp` and
`src/app/EditorUi.hpp`. The UI uses the official Dear ImGui SDL3/OpenGL3 backends;
the core library and its tests do not depend on Dear ImGui or OpenGL.

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
Dear ImGui is distributed under the [MIT license](https://github.com/ocornut/imgui/blob/v1.93.0/LICENSE.txt).

A source-code license has not been selected. Before distributing compiled releases, review the licenses and distribution conditions of the chosen dependencies and codecs. Only use media you have permission to distribute.

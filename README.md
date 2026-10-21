# MusDesk

**MusDesk** is a planned desktop tool for turning a music track and cover artwork into a vertical, animated music visualizer video for short-form platforms.

> **Project status: audio-reactive composition preview (M3).** MusDesk imports PNG, JPEG and still WebP artwork, plays MP3/WAV/OGG audio, and renders a rotating vinyl record, gently blurred moving background, audio-reactive blue rim, editable track labels and progress bar. The composition is rendered at 1080 × 1920 from an explicit timestamp. Presets and MP4 export are not implemented yet.

## What the app will do

1. Import a music track (MP3, WAV, or OGG) and cover artwork (PNG, JPEG, or WebP).
2. Display a spinning vinyl record with centered artwork, a blurred image background, track title, artist, and progress timer.
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

CMake downloads pinned SDL **3.4.18**, Dear ImGui **1.93.0**, stb and libwebp
**1.6.0** source archives from their official GitHub repositories and verifies
their SHA-256 checksums. stb decodes PNG/JPEG and rasterizes text; libwebp provides
the WebP decoder. Encoder tools are not built.
The first configuration needs internet access; sources and build products stay
inside the build directory. SDL's DLL and the `assets/Roboto-Medium.ttf` font
are copied next to `MusDesk.exe`; the remaining libraries are linked statically.
Keep the DLL and the assets directory alongside the executable when running it elsewhere.

The requested client size is 1280 × 800, with a minimum of 900 × 600. Desktop
scaling and window-manager constraints can affect physical size. Close the window
with the title-bar close button or **Alt+F4**. Rendering uses VSync when available,
a frame delay otherwise, and pauses while minimized. Diagnostic messages go to
the console; startup failures produce a nonzero exit code.

## Current workspace

- Use **Import artwork**, drop an image onto the window, or launch with
  `MusDesk.exe --artwork "path/to/artwork.png"`. Unicode paths are supported.
  Failed imports display an error and preserve the previous image.
- PNG, JPEG and still WebP are accepted up to 64 MiB, 8192 pixels per side and
  32 megapixels. Animated WebP is rejected; JPEG EXIF orientation is not applied.
- The right panel fits the **9:16** composition into the available space when the
  window is resized. The preview scale changes its display size, not the target
  1080 × 1920 format.
- Artwork fills the background and circular label through centered cropping,
  preserving its proportions. The record and label rotate together. The blue
  rim expands and brightens with the precomputed audio energy.
- Edit title, artist, rotation speed and preview time in the left panel.
  Without audio, a demo length can be adjusted;
  importing audio replaces it with the decoded duration. The composition
  contains no player transport buttons.
- Adjust preview scale, toggle safe-area guides, or press **Reset preview**.
  Guides belong only to the editor; reset preserves artwork and track labels.
  All settings last only for the session. Export remains disabled.
- Keyboard navigation is enabled: **Tab**, arrow keys, and **Space** can operate
  the controls. The controls panel scrolls when space is limited.

Window/context lifetime and the rendering loop are in `src/main.cpp`; editor
controls and the asynchronous native file dialog are in `src/app/`. Pure time
calculations live in `musdesk_core`, image decoding and audio metadata import in `musdesk_media`, and the
OpenGL framebuffer and composition drawing in `musdesk_render`. The renderer
does not depend on Dear ImGui. Its output depends on time and settings rather
than the order in which frames were requested.

### Audio tools and import

FFmpeg and ffprobe are free, open-source external tools. They are not downloaded
by CMake. Obtain a Windows build through the links on the
[FFmpeg download page](https://ffmpeg.org/download.html), then either place both
executables beside `MusDesk.exe`, add their directory to `PATH`, or set
`MUSDESK_FFMPEG_DIR` to the directory containing them before launching MusDesk.
The search order is the configured directory, executable directory, then `PATH`.
Each candidate is checked with `-version`; restart MusDesk after changing tools.
The panel reports missing or unusable tools without preventing artwork editing.

Use **Import audio**, drop an MP3/WAV/OGG file, or launch with
`MusDesk.exe --audio "path/to/track.mp3"`. `--artwork` and `--audio` can be combined.
Both tools are needed for preview import. ffprobe validates metadata; FFmpeg
decodes the first audio stream to 48 kHz stereo signed 16-bit PCM. The timeline
uses decoded sample count, which can differ slightly from container duration
because of codec padding. Failed or cancelled imports preserve the previous clip.
Detection, decoding and analysis run outside the render loop. Use **Cancel import**
to stop preparation; closing the editor also requests cancellation.

Imported audio starts paused. In **Preview audio**, use **Play/Pause**, **Restart**,
the seek slider, **Volume**, **Mute** and optional **Loop**. Dragging the timeline
temporarily pauses audio and resumes it on release if it was playing. Seeking
while paused keeps it paused. Without Loop, playback stops at the end; Play then
starts again from the beginning. Space toggles playback and Left/Right seek five
seconds when keyboard input is not captured by an editor control.

SDL requests PCM samples on its audio thread. Preview time follows consumed
input frames instead of a separate wall clock, avoiding cumulative audio/video
drift. Output-device buffering can introduce a small latency; sample-accurate
hardware presentation timing is not guaranteed. Audio continues while the window
is minimized, with rendering suspended. Volume and mute affect monitoring only.

RMS measures both stereo channels in 20 ms windows, normalizes energy against the
track peak with a minimum reference level, and applies 30 ms attack / 180 ms
release smoothing. The envelope is interpolated at the requested timestamp;
seeking reproduces the same rim, and pausing freezes the matching frame. This is
an energy visualizer rather than a beat detector or frequency spectrum.

PCM stays in memory for immediate seeking. Preview currently accepts tracks up
to 20 minutes, with a corresponding PCM byte limit and a 120-second decode limit.
Metadata/version processes retain a 15-second limit and 64 KiB output limit.
Arguments go directly through SDL's process API without a shell; Unicode paths
and spaces are supported. Only local files are accepted. Playback requires a
working audio output device; preparation errors are shown in the panel.

### Tests

The default CTest suite checks time formatting, deterministic animation values,
PNG/JPEG/WebP decoding, Unicode paths, alpha, malformed input and missing-tool
audio diagnostics, RMS smoothing and playback state using SDL's silent dummy
audio driver. To also register
the integration test requiring an OpenGL 3.3 graphics driver:

```powershell
cmake -S . -B build -DMUSDESK_ENABLE_RENDER_TESTS=ON
cmake --build build --config Debug --parallel 1 -- /nr:false
ctest --test-dir build -C Debug --output-on-failure
```

The render test checks 1080 × 1920 pixel readback, opacity, animation, preservation
of artwork after an invalid replacement, and identical pixels when seeking back
to the same time. It can also run against your own image:
`MusDeskRenderTests.exe "path/to/artwork.png"`. Pixel equality is checked within
one graphics context; identical results across different GPU drivers are not guaranteed.

For a focused audio integration check with installed tools, run
`MusDeskAudioTests.exe "path/to/track.mp3"`. Multiple audio paths can be supplied.
This checks detection, metadata, full PCM decoding, RMS generation, missing files
and damaged input without opening the editor. `MusDeskPlaybackTests` exercises
play/pause, sample-position seeks, end/restart, gain and invalid replacement;
CTest selects the dummy audio driver for this test.

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
stb offers [MIT or public-domain licensing](https://github.com/nothings/stb/blob/a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d/LICENSE).
libwebp uses a [BSD-style license](https://github.com/webmproject/libwebp/blob/v1.6.0/COPYING)
with a separate [patent grant](https://github.com/webmproject/libwebp/blob/v1.6.0/PATENTS).
The bundled Roboto font uses the [Apache 2.0 license](https://github.com/ocornut/imgui/blob/v1.93.0/docs/FONTS.md#fonts-included-in-this-repository).
FFmpeg and ffprobe are external tools with licensing determined by their build;
see the [FFmpeg license information](https://ffmpeg.org/legal.html).

A source-code license has not been selected. Before distributing compiled releases, review the licenses and distribution conditions of the chosen dependencies and codecs. Only use media you have permission to distribute.

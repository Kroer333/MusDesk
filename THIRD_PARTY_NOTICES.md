# Third-party components

MusDesk's own code currently has no selected license. This document records
dependency notices; it does not license MusDesk or private media files.

| Component | Pinned source | License / packaged notice |
| --- | --- | --- |
| SDL | 3.4.18, https://github.com/libsdl-org/SDL/tree/release-3.4.18 | zlib, `licenses/SDL.txt` |
| Dear ImGui and embedded default font | 1.93.0, https://github.com/ocornut/imgui/tree/v1.93.0 | MIT, `licenses/Dear-ImGui.txt` |
| stb_image / stb_truetype | a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d, https://github.com/nothings/stb | MIT or public domain, `licenses/stb.txt` |
| libwebp decoder | 1.6.0, 4fa21912338357f89e4fd51cf2368325b59e9bd9, https://github.com/webmproject/libwebp | BSD-style and patent grant, `licenses/libwebp.txt`, `licenses/libwebp-PATENTS.txt` |
| nlohmann/json | 3.12.0, https://github.com/nlohmann/json/tree/v3.12.0 | MIT, `licenses/nlohmann-json.txt` |
| Roboto Medium | Font distributed with Dear ImGui 1.93.0, Christian Robertson / Google | Apache 2.0, `licenses/Roboto-Apache-2.0.txt`, `licenses/Roboto-NOTICE.txt` |

The packaging step copies the dependency notices from the exact downloaded
sources rather than reconstructing their copyright statements.

## External media tools

FFmpeg and ffprobe are invoked as separate processes. MusDesk does not link
against FFmpeg libraries. The normal package omits both executables.
`MUSDESK_PACKAGE_MEDIA_TOOLS=ON` includes the developer's existing executables
and their selected build's license in a local evaluation ZIP.

FFmpeg's license depends on its enabled components; builds containing libx264
commonly enable GPL components. See [FFmpeg's license information](https://ffmpeg.org/legal.html).
The local evaluation option is not a public redistribution workflow. Preparing
public distribution of tool binaries also requires the corresponding source
and build information and checking the actual build's terms.

## Platform runtime

Windows builds use OpenGL supplied by Windows/the graphics driver. MSVC Release
runtime DLLs selected by CMake's InstallRequiredSystemLibraries are installed
app-locally when available. Their licensing remains Microsoft's; see the Visual
Studio redistribution terms. Windows system DLLs and graphics drivers are not
copied into the ZIP. Windows 10/11 x64 and a driver supporting OpenGL 3.3 are required.

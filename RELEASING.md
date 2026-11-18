# Release workflow

0.2.0 is already published. For the next release, update the version and release
notes first. Use a new tag and archive; preserve published releases.

## Build and check

From the repository root in a Visual Studio 2022 Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMUSDESK_PACKAGE_MEDIA_TOOLS=ON
cmake --build build --config Release --parallel 1 -- /nr:false
ctest --test-dir build -C Release --output-on-failure
cpack --config build/CPackConfig.cmake -C Release -D CPACK_PACKAGE_CHECKSUM=SHA256
```

Close the EXE being rebuilt. The private package requires FFmpeg/ffprobe in
`tools/ffmpeg/bin`, their build's `tools/ffmpeg/LICENSE` and source/build notes in
`tools/ffmpeg/README.txt`. Set the option to `OFF` for a tools-free ZIP.

Validate the newly generated archive (replace `<version>`):

```powershell
pwsh -NoProfile -File scripts/TestPackage.ps1 -Archive "build/packages/MusDesk-<version>-private-windows-x64.zip" -AllowPrivateTools
```

For a tools-free ZIP, omit `-private` and `-AllowPrivateTools`.
Extract to a fresh folder; check import, preview, export/cancel and external MP4
playback including the ending. Check clean Windows when available; document gaps.

## Publish through GitHub Desktop / GitHub

1. Review, commit and push source/docs through GitHub Desktop. Exclude build output and private media.
2. On GitHub, draft a release with `v<version>` targeting that commit.
3. Paste the release notes as text. Attach the matching ZIP and `.zip.sha256`.
4. Select **Pre-release** while acceptance checks remain incomplete; publish when reviewed.
5. Download the attachment and verify its checksum and startup.

Rebuild before publishing if packaged files changed. The tag and ZIP must match.
Release access follows repository visibility. Bundled tools retain their own license
requirements when shared; this personal packaging workflow does not assemble complete
Corresponding Source. See [PRIVATE_PACKAGE.txt](PRIVATE_PACKAGE.txt) and
[dependency notices](THIRD_PARTY_NOTICES.md).

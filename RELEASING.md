# Releasing MusDesk 0.1.0

## Build the personal private package

From the repository root, in a Visual Studio 2022 Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMUSDESK_PACKAGE_MEDIA_TOOLS=ON
cmake --build build --config Release --parallel 1 -- /nr:false
cpack --config build/CPackConfig.cmake -C Release -D CPACK_PACKAGE_CHECKSUM=SHA256
pwsh -NoProfile -File scripts/TestPackage.ps1 -Archive build/packages/MusDesk-0.1.0-private-windows-x64.zip -AllowPrivateTools
```

This package includes the existing FFmpeg/ffprobe executables, their GPLv3
license, upstream build/source information and binary hashes. It is prepared
for the owner's personal use/backup in a private repository. See
[PRIVATE_PACKAGE.txt](PRIVATE_PACKAGE.txt) for its scope. Repository privacy
does not waive license obligations when conveying copies to other people;
complete Corresponding Source for the tools and their linked libraries has
not been assembled by this packaging workflow.

The tools-free variant uses `-DMUSDESK_PACKAGE_MEDIA_TOOLS=OFF` and is named
`MusDesk-0.1.0-windows-x64.zip`. Validate it without `-AllowPrivateTools`.
The old `-local-win64.zip` is a legacy artifact; do not use it for this release.

## Acceptance checks

- Extract the complete ZIP into a fresh folder. Run MusDesk and confirm that
  both media tools report ready without any separate installation.
- Import artwork/audio and check preview, export and cancellation as appropriate.
- Play the resulting MP4 externally, including its start, seeks and ending.
- When possible, check a clean Windows machine with a suitable GPU driver.
- Keep remaining verification gaps in the release notes. If these checks remain
  incomplete, select **This is a pre-release** on GitHub.
- The application currently supports **English only**. Documentation language
  does not change the application interface.
- Keep the release ZIP consistent with the tagged source. Rebuild and repackage
  if source or packaged documentation changes after the archive was created.

## GitHub Desktop and private GitHub release

1. Review **Changes** in GitHub Desktop. Commit the project/license/documentation
   files, for example with `chore: prepare private MusDesk 0.1.0 release`.
   Do not commit `build`, external-tool binaries, exported videos or private media.
2. Select **Push origin**, then **Repository > View on GitHub**.
3. Confirm the repository is labelled **Private**. Creating a release does not
   make a repository private or change its visibility. If it is currently public,
   decide whether to change visibility in GitHub settings before uploading this
   personal package. Do not upload first and assume a release can be made private
   independently. Everyone with repository read access can also read its releases.
4. Confirm `main` contains the prepared commit and MIT `LICENSE`.
5. Open **Releases > Draft a new release**. In **Choose a tag**, create `v0.1.0`
   with **Target: main** containing that commit. Title: **MusDesk 0.1.0**.
   If the tag already exists, verify its commit before using it; do not move a
   tag belonging to a published release.
6. Paste `RELEASE_NOTES.md` into the description. Retain the English-only UI,
   private personal package scope and known limitations.
7. Attach exactly these two files from `build/packages`:
   - `MusDesk-0.1.0-private-windows-x64.zip`
   - `MusDesk-0.1.0-private-windows-x64.zip.sha256`
8. Select **This is a pre-release** while acceptance checks remain incomplete.
   Use **Save draft** to review the tag, description and attachments.
9. Select **Publish release** when ready. The release stays under the private
   repository's access rules; it does not become public merely by publishing it.
10. Download the attached ZIP, check SHA-256, extract it and run MusDesk again.
    Future fixes should receive a new version rather than replacing an issued ZIP.

GitHub Desktop handles the commit and push. The ZIP is a release attachment on
GitHub, not a file to add to the repository's Git history. Keeping the repository
private does not cancel permissions already granted to recipients under MIT/GPL.

GitHub documentation: [About releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases) and [Managing releases](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository).

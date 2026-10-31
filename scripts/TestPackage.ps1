param([Parameter(Mandatory=$true)][string]$Archive, [switch]$AllowPrivateTools, [switch]$AllowLocalTools)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archivePath = [IO.Path]::GetFullPath($Archive)
$expectedHash = ((Get-Content -LiteralPath "$archivePath.sha256" -Raw).Trim() -split '\s+')[0]
$actualHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
if ($actualHash -ine $expectedHash) { throw 'Package SHA-256 does not match.' }
$zip = [IO.Compression.ZipFile]::OpenRead($archivePath)
try {
    $files = @($zip.Entries | Where-Object { -not $_.FullName.EndsWith('/') } | ForEach-Object { $_.FullName })
    $roots = @($files | ForEach-Object { ($_ -split '/')[0] } | Select-Object -Unique)
    if ($roots.Count -ne 1 -or $roots[0] -notmatch '^MusDesk-\d+\.\d+\.\d+-(windows-x64|private-windows-x64|local-win64)$') {
        throw 'Unexpected package root directory.'
    }
    if ([IO.Path]::GetFileNameWithoutExtension($archivePath) -ne $roots[0]) {
        throw 'Archive name does not match the package root directory.'
    }
    $localPackage = $roots[0].EndsWith('-local-win64')
    $privatePackage = $roots[0].EndsWith('-private-windows-x64')
    if ($privatePackage -and -not $AllowPrivateTools) {
        throw 'Private packages with tools require -AllowPrivateTools and are intended for personal use in a private repository.'
    }
    if ($localPackage -and -not $AllowLocalTools) {
        throw 'Local evaluation packages must not be uploaded as public release assets. Use -AllowLocalTools only for local checks.'
    }
    $prefix = $roots[0] + '/'
    $packageVersion = [version]($roots[0] -replace '^MusDesk-(\d+\.\d+\.\d+)-.+$', '$1')
    $relative = @($files | ForEach-Object { $_.Substring($prefix.Length) })
    $required = @('MusDesk.exe','SDL3.dll','msvcp140.dll','vcruntime140.dll',
        'assets/Roboto-Medium.ttf','assets/MusDesk.png','README.md','CHANGELOG.md','THIRD_PARTY_NOTICES.md','LICENSE','LICENSE-STATUS.txt','RELEASE_NOTES.md',
        'presets/soft-glow.json','presets/beat-pulse.json',
        'licenses/SDL.txt','licenses/Dear-ImGui.txt',
        'licenses/stb.txt','licenses/libwebp.txt','licenses/libwebp-PATENTS.txt','licenses/nlohmann-json.txt',
        'licenses/Roboto-Apache-2.0.txt','licenses/Roboto-NOTICE.txt',
        'screenshots/workspace.png','screenshots/export.png','screenshots/README.md')
    if ($packageVersion -ge [version]'0.2.0') {
        $required += @('presets/side-kick.json','presets/spectrum-pulse.json')
    }
    foreach ($name in $required) {
        if ($relative -notcontains $name) { throw "Missing package file: $name" }
    }
    $mediaFiles = @('ffmpeg.exe','ffprobe.exe','licenses/FFmpeg-build.txt','licenses/FFmpeg-build-info.txt','licenses/FFmpeg-binaries.sha256','PRIVATE_PACKAGE.txt')
    if (-not ($localPackage -or $privatePackage) -and @($relative | Where-Object { $_ -in $mediaFiles }).Count) {
        throw 'The public release package must not contain locally bundled FFmpeg tools or their local build notices.'
    }
    if ($privatePackage) { $required += $mediaFiles }
    if ($localPackage -or $privatePackage) {
        $requiredMedia = if ($privatePackage) { $mediaFiles } else { @('ffmpeg.exe','ffprobe.exe','licenses/FFmpeg-build.txt') }
        foreach ($name in $requiredMedia) {
            if ($relative -notcontains $name) { throw "Incomplete local media tools: $name" }
        }
    }
    foreach ($name in $relative) {
        $allowed = $name -match '^(MusDesk\.exe|SDL3\.dll|(?:msvcp140[^/]*|vcruntime140[^/]*|concrt140)\.dll|ffmpeg\.exe|ffprobe\.exe|README\.md|CHANGELOG\.md|THIRD_PARTY_NOTICES\.md|LICENSE-STATUS\.txt)$' `
            -or $required -contains $name -or $name -in @('licenses/FFmpeg-build.txt','licenses/FFmpeg-build-info.txt')
        if (-not $allowed -or $name -match '(^|/)\.\.(/|$)') { throw "Unexpected package entry: $name" }
    }
    if ($privatePackage) {
        $hashReader = [IO.StreamReader]::new($zip.GetEntry($prefix + 'licenses/FFmpeg-binaries.sha256').Open())
        try { $toolHashes = $hashReader.ReadToEnd() -split '\r?\n' | Where-Object { $_ } } finally { $hashReader.Dispose() }
        if ($toolHashes.Count -ne 2) { throw 'Expected exactly two bundled tool hashes.' }
        foreach ($tool in @('ffmpeg.exe','ffprobe.exe')) {
            $line = @($toolHashes | Where-Object { $_ -cmatch ('^[a-fA-F0-9]{64}  ' + [regex]::Escape($tool) + '$') })
            if ($line.Count -ne 1) { throw "Missing or duplicate bundled tool hash: $tool" }
            $toolStream = $zip.GetEntry($prefix + $tool).Open()
            $sha256 = [Security.Cryptography.SHA256]::Create()
            try { $toolHash = [Convert]::ToHexString($sha256.ComputeHash($toolStream)) }
            finally { $toolStream.Dispose(); $sha256.Dispose() }
            if ($toolHash -ine ($line[0] -split '\s+')[0]) { throw "Bundled tool hash mismatch: $tool" }
        }
    }
    Write-Output "Package layout and SHA-256 verified: $($relative.Count) files."
} finally { $zip.Dispose() }

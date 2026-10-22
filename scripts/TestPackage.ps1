param([Parameter(Mandatory=$true)][string]$Archive)
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
    if ($roots.Count -ne 1 -or $roots[0] -notmatch '^MusDesk-\d+\.\d+\.\d+-local-win64$') {
        throw 'Unexpected package root directory.'
    }
    $prefix = $roots[0] + '/'
    $relative = @($files | ForEach-Object { $_.Substring($prefix.Length) })
    $required = @('MusDesk.exe','SDL3.dll','msvcp140.dll','vcruntime140.dll',
        'assets/Roboto-Medium.ttf','assets/MusDesk.png','README.md','CHANGELOG.md','THIRD_PARTY_NOTICES.md','LICENSE-STATUS.txt',
        'presets/soft-glow.json','presets/beat-pulse.json','licenses/SDL.txt','licenses/Dear-ImGui.txt',
        'licenses/stb.txt','licenses/libwebp.txt','licenses/libwebp-PATENTS.txt','licenses/nlohmann-json.txt',
        'licenses/Roboto-Apache-2.0.txt','licenses/Roboto-NOTICE.txt',
        'screenshots/workspace.png','screenshots/export.png','screenshots/README.md')
    foreach ($name in $required) {
        if ($relative -notcontains $name) { throw "Missing package file: $name" }
    }
    if ($relative -contains 'ffmpeg.exe' -or $relative -contains 'ffprobe.exe') {
        foreach ($name in @('ffmpeg.exe','ffprobe.exe','licenses/FFmpeg-build.txt')) {
            if ($relative -notcontains $name) { throw "Incomplete local media tools: $name" }
        }
    }
    foreach ($name in $relative) {
        $allowed = $name -match '^(MusDesk\.exe|SDL3\.dll|(?:msvcp140[^/]*|vcruntime140[^/]*|concrt140)\.dll|ffmpeg\.exe|ffprobe\.exe|README\.md|CHANGELOG\.md|THIRD_PARTY_NOTICES\.md|LICENSE-STATUS\.txt)$' `
            -or $required -contains $name -or $name -in @('licenses/FFmpeg-build.txt','licenses/FFmpeg-build-info.txt')
        if (-not $allowed -or $name -match '(^|/)\.\.(/|$)') { throw "Unexpected package entry: $name" }
    }
    Write-Output "Package layout and SHA-256 verified: $($relative.Count) files."
} finally { $zip.Dispose() }

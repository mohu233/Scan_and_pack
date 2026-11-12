$ErrorActionPreference = 'Stop'
function Get-ArchiveHash([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
    finally { $stream.Dispose(); $hasher.Dispose() }
}
$version = '1.1.3'
$expectedHash = 'bce041ea9fe0f8a3351301dcbe29cdf6a523bb25cf9c62f17ebb5699a8051d0f'
$expectedExecutableHash = 'b176b561fd8bf15d828fcab484cfd5b4fb941cb9f61807901ca64b955af27e1f'
$destination = Join-Path $PSScriptRoot 'tools\wireproxy'
$downloadDirectory = Join-Path $PSScriptRoot 'build\downloads'
New-Item -ItemType Directory -Path $destination, $downloadDirectory -Force | Out-Null
$archive = Join-Path $downloadDirectory "wireproxy-$version-windows-amd64.tar.gz"
if (-not (Test-Path -LiteralPath $archive) -or (Get-ArchiveHash $archive) -ne $expectedHash) {
    Invoke-WebRequest -Uri "https://github.com/windtf/wireproxy/releases/download/v$version/wireproxy_windows_amd64.tar.gz" -OutFile $archive -UseBasicParsing
}
if ((Get-ArchiveHash $archive) -ne $expectedHash) { throw 'Wireproxy archive checksum mismatch.' }
$executable = Join-Path $destination 'wireproxy.exe'
if (-not (Test-Path -LiteralPath $executable) -or (Get-ArchiveHash $executable) -ne $expectedExecutableHash) {
    $extractPath = Join-Path $downloadDirectory ('extract-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $extractPath | Out-Null
    try {
        & tar -xf $archive -C $extractPath
        $extracted = Join-Path $extractPath 'wireproxy.exe'
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $extracted)) { throw 'Wireproxy extraction failed.' }
        if ((Get-ArchiveHash $extracted) -ne $expectedExecutableHash) { throw 'Wireproxy executable checksum mismatch.' }
        Copy-Item -LiteralPath $extracted -Destination $executable -Force
    } finally {
        $resolvedExtract = (Resolve-Path -LiteralPath $extractPath).Path
        if ((Split-Path -Parent $resolvedExtract) -ne (Resolve-Path -LiteralPath $downloadDirectory).Path) { throw 'Unexpected extraction directory.' }
        Remove-Item -LiteralPath $resolvedExtract -Recurse -Force
    }
}
Write-Output "Verified wireproxy v$version (Windows amd64) in $destination"

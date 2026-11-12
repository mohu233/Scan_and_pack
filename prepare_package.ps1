param([Parameter(Mandatory = $true)][string]$ProjectDir)
$ErrorActionPreference = 'Stop'
$projectPath = (Resolve-Path -LiteralPath $ProjectDir).Path.TrimEnd('\')
$buildPath = [IO.Path]::GetFullPath((Join-Path $projectPath 'build'))
$packagePath = [IO.Path]::GetFullPath((Join-Path $buildPath 'package-root'))
if (-not $packagePath.StartsWith($projectPath + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Package directory is outside the project.' }
if (Test-Path -LiteralPath $packagePath) {
    $existing = Get-Item -LiteralPath $packagePath -Force
    if ($existing.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Refusing to clean a linked package directory.' }
    $resolvedPackage = (Resolve-Path -LiteralPath $packagePath).Path
    if ($resolvedPackage -ne $packagePath -or (Split-Path -Parent $resolvedPackage) -ne $buildPath) { throw 'Unexpected package directory.' }
    Remove-Item -LiteralPath $resolvedPackage -Recurse -Force
}
New-Item -ItemType Directory -Path $packagePath -Force | Out-Null

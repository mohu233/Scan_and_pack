param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,

    [Parameter(Mandatory = $true)]
    [string]$OutputExe,

    [string]$Version = ""
)

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'VERSION.txt') -Raw -ErrorAction Stop).Trim()
}
if ($Version -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?$') {
    throw 'Version must contain three or four numeric components.'
}

$packagePath = (Resolve-Path -LiteralPath $PackageDir -ErrorAction Stop).Path
$outputPath = [IO.Path]::GetFullPath($OutputExe)
$outputDirectory = Split-Path -Parent $outputPath
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

$template = @'
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
Name "ProfilePower"
OutFile "{{OUTPUT_EXE}}"
InstallDir "$PROGRAMFILES64\ProfilePower"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_ICONSTOP "ProfilePower requires 64-bit Windows."
        Abort
    ${EndIf}
    SetRegView 64
    SetShellVarContext all
FunctionEnd

Function un.onInit
    SetRegView 64
    SetShellVarContext all
FunctionEnd

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"

Section "Install"
    SetOutPath $INSTDIR
    File /r /x installer.nsi "{{PACKAGE_DIR}}\*.*"
    CreateShortCut "$DESKTOP\ProfilePower.lnk" "$INSTDIR\ProfilePower.exe"
    CreateDirectory "$SMPROGRAMS\ProfilePower"
    CreateShortCut "$SMPROGRAMS\ProfilePower\ProfilePower.lnk" "$INSTDIR\ProfilePower.exe"
    CreateShortCut "$SMPROGRAMS\ProfilePower\Uninstall.lnk" "$INSTDIR\uninstall.exe"
    WriteUninstaller "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProfilePower" "DisplayName" "ProfilePower"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProfilePower" "UninstallString" '$\"$INSTDIR\uninstall.exe$\"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProfilePower" "DisplayIcon" "$INSTDIR\ProfilePower.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProfilePower" "Publisher" "ProfilePower"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProfilePower" "DisplayVersion" "{{VERSION}}"
SectionEnd

Section "Uninstall"
    Delete "$DESKTOP\ProfilePower.lnk"
    RMDir /r "$SMPROGRAMS\ProfilePower"
    RMDir /r "$INSTDIR"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProfilePower"
SectionEnd
'@

$script = $template.Replace("{{PACKAGE_DIR}}", $packagePath)
$script = $script.Replace("{{OUTPUT_EXE}}", $outputPath)
$script = $script.Replace("{{VERSION}}", $Version)
$scriptPath = Join-Path $packagePath "installer.nsi"
[IO.File]::WriteAllText($scriptPath, $script, (New-Object Text.UTF8Encoding($true)))
Write-Host "NSIS script written to: $scriptPath"

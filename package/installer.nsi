Unicode True
!include "MUI2.nsh"

Name "扫码入库"
OutFile "扫码入库安装程序_V1.0.4.exe"
InstallDir "$LOCALAPPDATA\Programs\扫码入库"
RequestExecutionLevel user
SetCompressor /SOLID lzma
VIProductVersion "1.0.4.0"
VIAddVersionKey /LANG=2052 "ProductName" "Scan Packing"
VIAddVersionKey /LANG=2052 "FileDescription" "Scan Packing Setup"
VIAddVersionKey /LANG=2052 "FileVersion" "1.0.4"
VIAddVersionKey /LANG=2052 "ProductVersion" "1.0.4"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\扫码入库.exe"
!define MUI_FINISHPAGE_RUN_TEXT "立即运行扫码入库"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"

Section "安装" SEC_MAIN
    SetOutPath "$INSTDIR"
    File /r "app\*.*"
    WriteUninstaller "$INSTDIR\卸载.exe"
    CreateDirectory "$SMPROGRAMS\扫码入库"
    CreateShortcut "$SMPROGRAMS\扫码入库\扫码入库.lnk" "$INSTDIR\扫码入库.exe"
    CreateShortcut "$SMPROGRAMS\扫码入库\卸载.lnk" "$INSTDIR\卸载.exe"
    CreateShortcut "$DESKTOP\扫码入库.lnk" "$INSTDIR\扫码入库.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\扫码入库" "DisplayName" "扫码入库"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\扫码入库" "UninstallString" '"$INSTDIR\卸载.exe"'
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\扫码入库" "DisplayVersion" "1.0.4"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\扫码入库" "Publisher" "扫码入库"
SectionEnd

Section "Uninstall"
    Delete "$DESKTOP\扫码入库.lnk"
    RMDir /r "$SMPROGRAMS\扫码入库"
    RMDir /r "$INSTDIR"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\扫码入库"
SectionEnd

@echo off
setlocal

for %%I in ("%~dp0.") do set "PROJECT_DIR=%%~fI"
set "BUILD_DIR=%PROJECT_DIR%\build\package"
set "PKG_DIR=%PROJECT_DIR%\build\package-root"
set "OUTPUT=%PROJECT_DIR%\build\ProfilePower_Setup.exe"

if defined QT_DIR set "PATH=%QT_DIR%\bin;%PATH%"
if not defined NSIS set "NSIS=%ProgramFiles(x86)%\NSIS\makensis.exe"

where qmake >nul 2>&1 || (echo ERROR: qmake was not found. Set QT_DIR or update PATH. & exit /b 1)
where mingw32-make >nul 2>&1 || (echo ERROR: mingw32-make was not found on PATH. & exit /b 1)
where windeployqt >nul 2>&1 || (echo ERROR: windeployqt was not found on PATH. & exit /b 1)
if not exist "%NSIS%" (echo ERROR: NSIS was not found at "%NSIS%". & exit /b 1)

powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_DIR%\fetch_wireproxy.ps1"
if errorlevel 1 exit /b 1

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"

echo [1/5] Configuring release build...
qmake "%PROJECT_DIR%\ProfilePower.pro" CONFIG+=release CONFIG-=debug
if errorlevel 1 goto :fail

echo [2/5] Building ProfilePower...
mingw32-make -j4
if errorlevel 1 goto :fail

set "APP_EXE=%BUILD_DIR%\release\ProfilePower.exe"
if not exist "%APP_EXE%" set "APP_EXE=%BUILD_DIR%\ProfilePower.exe"
if not exist "%APP_EXE%" (echo ERROR: ProfilePower.exe was not produced. & goto :fail)

echo [3/5] Preparing package directory...
powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_DIR%\prepare_package.ps1" -ProjectDir "%PROJECT_DIR%"
if errorlevel 1 goto :fail
copy /y "%APP_EXE%" "%PKG_DIR%\ProfilePower.exe" >nul
copy /y "%PROJECT_DIR%\proxy_relay.py" "%PKG_DIR%\proxy_relay.py" >nul
copy /y "%PROJECT_DIR%\proxy_relay_v2.py" "%PKG_DIR%\proxy_relay_v2.py" >nul
copy /y "%PROJECT_DIR%\browser_cdp_runtime.py" "%PKG_DIR%\browser_cdp_runtime.py" >nul
mkdir "%PKG_DIR%\tools\wireproxy"
copy /y "%PROJECT_DIR%\tools\wireproxy\wireproxy.exe" "%PKG_DIR%\tools\wireproxy\wireproxy.exe" >nul
if errorlevel 1 goto :fail
copy /y "%PROJECT_DIR%\tools\wireproxy\LICENSE.txt" "%PKG_DIR%\tools\wireproxy\LICENSE.txt" >nul
if errorlevel 1 goto :fail
copy /y "%PROJECT_DIR%\tools\wireproxy\NOTICE.txt" "%PKG_DIR%\tools\wireproxy\NOTICE.txt" >nul
if errorlevel 1 goto :fail
windeployqt --release --no-translations "%PKG_DIR%\ProfilePower.exe"
if errorlevel 1 goto :fail

echo [4/5] Generating installer script...
powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_DIR%\write_nsis.ps1" -PackageDir "%PKG_DIR%" -OutputExe "%OUTPUT%"
if errorlevel 1 goto :fail

echo [5/5] Building installer...
"%NSIS%" "%PKG_DIR%\installer.nsi"
if errorlevel 1 goto :fail

popd
echo Installer created: %OUTPUT%
exit /b 0

:fail
popd
echo ERROR: Packaging failed.
exit /b 1

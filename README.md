# Scan and Pack

[简体中文](README.zh-CN.md)

Scan and Pack is a Windows desktop application for warehouse packing operations. It receives 10-character Amazon FNSKU values from a barcode scanner, assigns scanned units to box numbers, compares packed quantities against an imported packing plan, and exports packing records to Excel.

The interface uses a light industrial dashboard design so operators can quickly see the current box, latest scan, packed quantities, and plan differences in a warehouse or workshop environment.

## Features

- Receives 10-character FNSKU scans and clears the input automatically.
- Requires a box number before accepting a scan.
- Groups scan history by box and FNSKU, with the latest activity shown first.
- Shows large, high-contrast quantities and supports removing one scanned unit.
- Imports an Excel packing plan containing `SKU` and `Quantity`.
- Resolves both SKU and MSKU aliases to the same FNSKU. This handles names that differ only because one system replaces spaces with underscores.
- Displays plan quantity, packed quantity, and a signed difference:
  - over-packed: `+N`
  - under-packed: `-N`
  - complete: `0`
- Imports and exports `.xlsx` packing lists, including Excel shared-string files.
- Saves box numbers and timestamps in exported packing lists.
- Updates the FNSKU/SKU index from Lingxing ERP with automatic pagination.
- Stores the Lingxing token and generated index outside the installation directory.
- Plays an offline Chinese “packed” announcement after a successful scan.
- Defaults file import and export dialogs to the Windows desktop.

## Data location

Runtime data is stored in the current user's temporary directory:

```text
%TEMP%\扫码入库\
├── token.txt
└── fnsku_sku_index.xlsx
```

Both files are created automatically when missing. Paste only the Lingxing ERP `auth-token` value into `token.txt`. Never commit tokens or private index data to Git.

## Requirements

- Windows 10 or Windows 11
- Qt 6.11.1 or a compatible Qt 6 installation
- MinGW 13.1 64-bit
- Qt modules: Widgets, Network, OpenGL Widgets, and Core private headers
- NSIS, only when building the Windows installer

## Build with Qt Creator

1. Open `untitled.pro` in Qt Creator.
2. Select a Qt 6 MinGW 64-bit kit.
3. Configure either Debug or Release.
4. Build and run the project.
5. Copy `assets/packed.wav` next to the executable when running outside the packaged application.

## Command-line build

Run the following commands from a separate build directory with Qt and MinGW available in `PATH`:

```powershell
qmake ..\..\untitled.pro -spec win32-g++ "CONFIG+=release"
mingw32-make -j1 release
```

The project links against the Windows multimedia library for offline audio playback.

## Packaging

The NSIS source is located at `package/installer.nsi`. Before building the installer:

1. Build the Release executable.
2. Copy it to `package/app/扫码入库.exe`.
3. Copy `assets/packed.wav` to `package/app/packed.wav`.
4. Deploy the required Qt runtime libraries with `windeployqt`.
5. Compile the installer script using UTF-8 input:

```powershell
makensis /INPUTCHARSET UTF8 package\installer.nsi
```

Compiled applications, Qt runtime files, build directories, and installer executables are intentionally excluded from Git.

## Project structure

```text
Scan_and_pack/
├── assets/                Offline audio resources
├── package/installer.nsi  NSIS installer definition
├── main.cpp               Application entry point
├── mainwindow.cpp         Business logic and UI behavior
├── mainwindow.h           Main window declarations
├── mainwindow.ui          Qt Designer interface
├── untitled.pro           qmake project configuration
└── untitled_zh_CN.ts      Qt translation source
```

## Security notes

- Do not commit `token.txt`, cookies, authentication headers, or generated seller index files.
- Treat exported packing lists as operational business data.
- If the Lingxing token expires, update the local `token.txt` through the application.


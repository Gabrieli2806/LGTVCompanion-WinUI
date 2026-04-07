# Build Instructions

This guide explains how to build and test LGTV Companion from source.

## Prerequisites

| Tool | Required | Download |
|------|----------|----------|
| **Windows 10/11** | Yes | — |
| **Visual Studio 2022** | Yes | [Download](https://visualstudio.microsoft.com/downloads/) |
| **Desktop development with C++ workload** | Yes | Install via VS Installer |
| **Windows App SDK / WinUI 3 (C++)** | For WinUI project | VS Installer → Workloads → check "Windows application development", or Individual Components → search "Windows App SDK C++ Templates" |
| **Vcpkg** | Yes | [Install guide](https://github.com/microsoft/vcpkg#quick-start-windows) |
| **HeatWave for VS2022** | Only for installer (.msi) | [Download](https://www.firegiant.com/docs/heatwave/) |

## Quick Start (Command Line)

A one-command build script is provided for convenience:

```bat
build.bat
```

This will install dependencies via vcpkg and build all projects in Release|x64 configuration. See `build.bat --help` for options.

### Manual command-line build

```bat
:: 1. Clone and enter the repo
git clone https://github.com/JPersson77/LGTVCompanion.git
cd LGTVCompanion

:: 2. Install vcpkg (if you don't have it already)
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
set VCPKG_ROOT=C:\vcpkg

:: 3. Integrate vcpkg with MSBuild
vcpkg integrate install

:: 4. Build (Release, x64)
msbuild /p:VcpkgEnableManifest=true /p:Configuration=Release /p:Platform=x64 /Restore LGTVCompanion.sln
```

The built binaries will be in `x64\Release\` under each project folder.

## Visual Studio (IDE) Build

1. Open `LGTVCompanion.sln` in Visual Studio 2022.
2. Ensure vcpkg is integrated: run `vcpkg integrate install` in a terminal.
3. For each project, open **Project → Properties → Vcpkg** and set **"Use Manifest"** to **Yes** (for both Debug/Release and x64/ARM64 configurations).
4. Select the desired configuration (Debug or Release) and platform (x64 or ARM64) from the toolbar.
5. Build the solution (**Build → Build Solution** or `Ctrl+Shift+B`).

## Dependencies (via vcpkg)

A `vcpkg.json` manifest is included in the repository root. The following dependencies are automatically resolved:

- `nlohmann-json` — JSON parsing
- `boost-asio` — Async I/O
- `boost-beast` — WebSocket/HTTP
- `openssl` — TLS/SSL
- `phnt` — Windows NT headers

### Manual dependency install (alternative to manifest mode)

```bat
vcpkg install nlohmann-json:x64-windows-static
vcpkg install boost-asio:x64-windows-static
vcpkg install boost-beast:x64-windows-static
vcpkg install openssl:x64-windows-static
vcpkg install phnt:x64-windows-static

:: For ARM64 cross-compilation:
vcpkg install nlohmann-json:arm64-windows-static
vcpkg install boost-asio:arm64-windows-static
vcpkg install boost-beast:arm64-windows-static
vcpkg install openssl:arm64-windows-static
vcpkg install phnt:arm64-windows-static
```

## Project Structure

| Project | Output | Description |
|---------|--------|-------------|
| **LGTV Companion UI** | `LGTV Companion.exe` | Legacy Win32 user interface |
| **LGTV Companion UI WinUI** | `LGTV Companion WinUI.exe` | New WinUI 3 user interface |
| **LGTV Companion Service** | `LGTVsvc.exe` | Windows service for power management |
| **LGTV Companion Daemon** | `LGTVdaemon.exe` | User-mode background daemon |
| **LGTV Companion Console** | `LGTVcli.exe` | Command-line control tool |
| **LGTV Companion Updater** | `LGTVupdater.exe` | Auto-updater component |
| **LGTV Companion Setup** | `LGTV Companion Setup.msi` | WiX installer (requires HeatWave) |

## Testing

This project currently relies on manual testing with a physical LG WebOS TV on the local network.

### Quick test after building

1. Build the solution (Debug or Release).
2. Run the **UI** executable (`LGTV Companion.exe` or `LGTV Companion WinUI.exe`) from the output directory.
3. Click **Scan** to discover LG devices on your network.
4. Configure a device and click **Test** from the drop-down menu to verify power on/off control.

### Testing via CLI

```bat
:: Power off a device
LGTVcli.exe -poweroff Device1

:: Power on a device
LGTVcli.exe -poweron Device1

:: See all available commands
LGTVcli.exe -help
```

Refer to [Commandline.md](Commandline.md) for the full list of CLI commands.

### CI Testing

The project uses GitHub Actions for CI. A build is triggered automatically on:
- Pushes of version tags (`v*`)
- Pull requests to the `main` branch
- Manual workflow dispatch

You can trigger a CI build by creating a pull request or using the **Actions** tab in GitHub.

## Building the installer (.msi)

1. Install the [HeatWave](https://www.firegiant.com/docs/heatwave/) extension for VS2022.
2. Build the full solution in Release configuration.
3. The installer is output to `LGTV Companion Setup\bin\<platform>\Release\`.

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `vcpkg` not found | Ensure `VCPKG_ROOT` is set and vcpkg is in your PATH |
| NuGet restore fails | Run `msbuild /Restore` before building |
| WinUI project fails | Install Windows App SDK via VS Installer → Individual Components |
| Missing `phnt` | Update vcpkg: `cd %VCPKG_ROOT% && git pull && bootstrap-vcpkg.bat` |
| Setup project fails | Install the HeatWave VS extension; it is only needed for the .msi package |

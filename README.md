# MCM Bridge

Native MCM host for FLICK and SKSE Menu Framework. Existing MCMs keep their original callbacks, with a dynamic registry and no fixed limit on the number of MCMs. MCMMemory handles profiles, recording, backup and restore through the host API.

The project owner has repeatedly tested their SkyUI installation and MCMMemory integration successfully in-game. This is not a compatibility claim for every runtime or mod. VR and custom SWF/DDS rendering are not supported. This release is the SkyUI add-on, not the standalone installation.

## Requirements

- Skyrim SE or AE, matching SKSE, and Address Library for SKSE Plugins
- FLICK with API 5 or newer, or SKSE Menu Framework 3.18 or newer, installed separately
- SkyUI for the current SkyUI add-on package
- MCM Helper, required even if your individual MCMs do not use it
- Current [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vc14/vc_redist.x64.exe)

Supported Helper release binaries: 1.4.0 AE, 1.4.0 SE backport, 1.5.0, 1.6.2, 1.6.3 and 1.6.3 SE 1.5.97 backport. Choose the variant matching your Skyrim runtime. Other or modified binaries are rejected; this is not a blanket "1.4.0 or newer" rule.

## Installation

Install the complete package with your mod manager. Let MCM Bridge win conflicts for its four PEX scripts; keep SkyUI's plugin and other features. Restart Skyrim after updating and preserve your existing MCMBridge.ini. Back up your saves.

MCM Unlocked, MCM Menu Redone, Menu Maid 2, MCM super SEEDED and an active McmRecorder.esp are incompatible. A detected conflict shows a Windows dialog and disables MCM Bridge for that session. Ignore continues without the Bridge; it does not guarantee a working MCM fallback. For Recorder profiles, use [MCMMemory](https://www.nexusmods.com/skyrimspecialedition/mods/189722).

Missing or overwritten host scripts also produce an error. If you previously used MCM Unlocked, disable it completely, reinstall MCM Bridge and make sure Bridge's PEX files are not overwritten. Existing saves may retain its old manager.

## Settings

The Journal's Mod Configuration entry always opens the selected frontend. FLICK is preferred when both frontends are available; change `Prefer FLICK` in Settings to switch after current operations finish. `MCM Bridge > Settings` also controls Journal closing and the pause during changes. `MCM Bridge > Browser` provides search, display-name aliases, the optional MCMs folder and editable alphabetical ranges. FLICK uses flat groups such as `MCM - A-C`, with page selection inside each MCM; Menu Framework uses nested folders. Aliases never change original MCM names or MCMMemory identities. Root display names start with an uppercase letter.

Settings are saved in `Data/SKSE/Plugins/MCMBridge.ini`. Manual edits require a restart. Defaults:

```ini
[General]
PauseDuringWrites=true
CloseJournalOnRedirect=true
PreferFLICK=true
GroupMCMs=false
AlphabeticMCMs=false
MCMRangeEnds=CGLRZ
```

`MCMRangeEnds=CGLRZ` means A-C, D-G, H-L, M-R, S-Z. The Browser can edit or balance these ranges. Custom content currently shows a placeholder.

MCMMemory is optional and installed separately. Its build must implement the [host contract](include/MCMBridge/API/MCMBridgeHost.h). A stock Memory build without that integration is not supported. Memory owns profiles and restore results. Memory's own interface still requires SKSE Menu Framework, even when Bridge displays MCMs in FLICK.

## Build

Requires Visual Studio 2026 (v145), CMake 3.28+ and vcpkg. Builds use the included PEX files; their PSC sources are also in the repository. Caprica 0.3.0 is needed only when changing those scripts. Set VCPKG_ROOT to your vcpkg checkout. Use clang-format 22.1.3 for source-format checks.

```text
cmake --preset FLATRIM
cmake --build --preset FLATRIM-Debug
ctest --preset FLATRIM-Debug
cmake --build --preset FLATRIM-Release
ctest --preset FLATRIM-Release
cmake --build --preset FLATRIM-Release --target format-check
cpack --config build/CPackConfig.cmake -C Release
```

For test packages, configure with `-DMCM_BRIDGE_TEST_RC=1` to label the DLL and package `1.1.0rc1`; increment the candidate for subsequent test builds. Zero selects the stable version.

Tag pushes build and test the project, then upload the runtime ZIP and its SHA256 as workflow artifacts. Tags must match the CMake version, for example `v1.1.0`. No GitHub Release is created automatically. Framework and Memory are not bundled.

GPL-3.0-only. See [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md).

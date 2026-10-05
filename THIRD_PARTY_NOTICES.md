# Third-Party Notices

## SkyUI MCM compatibility contract

The native Papyrus facade preserves function/event declarations, constants and
field layouts from the local SkyUI Community MCM reference. Its implementation
routes to MCMBridge's native storage and retains the original callback protocol.
SkyUI interface assets and compiled reference scripts are not bundled by the
native-facades build target. Reference scripts are compile-time imports only.

## SKSE Menu Framework 3

The SDK header is pinned from QTR-Modding/SKSE-Menu-Framework-3 at commit:

`c8cfc5c93fa3b5f6261cef695ab814e4467dd980`

The upstream project is licensed under GPL-3.0.

## MCMMemory

The discovery, Papyrus continuation, and headless MCM sequencing design was informed by MCMMemory at commit:

`31156261588b896f7bb8114d1efaa53dba6fb7ab`

MCMMemory is licensed under GPL-3.0. MCM Bridge uses independently structured implementations and preserves attribution for adapted patterns.

## MCM Helper

MCM Helper behavior and configuration formats were studied at commit:

`a30334864ea46ab6ee9e74bca06187630b67c039`

MCM Helper is licensed under MIT.

The native capture uses matched release DLLs and PDBs for 1.4.0 (AE and SE
backport), 1.5.0, 1.6.2 and 1.6.3 to verify function boundaries and argument
layouts. The v1.4.0/v1.5.0 source contracts use coroutines; the newer releases
use callbacks. Optional regression tests compile original coroutine headers from
the local reference checkout; those headers are not included in the shipping DLL.

## Mutagen (development validation only)

The optional standalone bootstrap validator uses Mutagen.Bethesda.Skyrim 0.51.3,
licensed GPL-3.0-only. It independently reads generated plugin records; no
Mutagen code or binaries are linked into MCMBridge or included in runtime packages.
Project: https://github.com/Mutagen-Modding/Mutagen

## MinHook

MinHook 1.3.4 is linked statically through the pinned vcpkg baseline. It relocates
the verified Helper entry points when installing the native host adapter.
Copyright (C) 2009-2017 Tsuda Kageyu; disassembler portions Copyright (c)
2008-2009 Vyacheslav Patkov. See [the full license](licenses/MinHook.txt).

## MCM Unlocked

Earlier registry integration used SkyrimSE_MCMUnlocked 2.1.6 at commit:

`3ef5dd6a35dc18338040c6d652e759b232f99687`

MCM Unlocked is licensed under GPL-3.0.

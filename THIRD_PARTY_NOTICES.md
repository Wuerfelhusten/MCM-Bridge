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

## FLICK

The FLICK API headers are fetched from Fuzzlesz/FUCK_API at commit
`a9ce5d17ebe095e3f11e2d9acd34534fd3d066c9`. FLICK is GPL-3.0-licensed.
Its matching ImGui header is fetched from powerof3/imgui at commit
`fbbe3efd107e960f864d2944fdf280b465110bad` (MIT). Drawing uses FLICK's function
table; no second ImGui implementation or FLICK binary is bundled.

## Reset icon

FLICK renders the same Font Awesome Free Solid U+F0E2 outline used by SKSE
Menu Framework, rasterized from its font glyph rather than redrawn by hand.
Copyright Font Awesome / Fonticons, Inc. The outline is SIL OFL 1.1-licensed;
see [the full license](licenses/FontAwesome.txt). The license is also embedded
in the DLL's `ResetIconLicense` resource. No font atlas or font file is shared
between the frontends. Only the derived reset texture is added to the runtime
package; FLICK loads and owns its image resource.

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

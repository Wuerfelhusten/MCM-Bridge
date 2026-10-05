# MCM Feature Lab

Non-shipping source fixtures for Classic SkyUI, MCM Helper and nl_mcm.
No production plugin code is changed. Use a disposable save.

## Status

Source kit, not an installable mod. No ESP, compiled PEX or in-game validation
is included. Passing the structural checker does not prove Papyrus compilation
or MCM Bridge compatibility. Unsupported Bridge controls remain intentional
test cases; do not disable or remove them to make the tests pass.

"All functions" means the public control families and representative lifecycle,
navigation, persistence and presentation features listed below. Arbitrary custom
SWF behavior, every flag combination and every third-party extension cannot be
exhaustively covered by a finite fixture. Uncovered boundaries are listed below.

## Setup

Build three separate, non-ESL test plugins in the Creation Kit:

| Plugin | Record | Scripts and properties |
| --- | --- | --- |
| MBLClassic.esp | Start Game Enabled quest, editor ID MBLClassicQuest | Attach MBLClassic only. MBLClassicStates is its script parent. |
| MBLHelper.esp | Start Game Enabled quest, local form ID 800 | Attach MBLHelper. Keep the exact plugin name and ID or update JSON references. |
| MBLHelper.esp | Float Global, local form ID 801 | Initial value 2.0. Used only by the GlobalValue test. |
| MBLNL.esp | Start Game Enabled quest, editor ID MBLNLQuest | Attach upstream nl_mcm, MBLNLControls, MBLNLNavigation and MBLNLExtra. |

For NL set ModName to "Feature Lab NL" on nl_mcm.
Set LandingPageName to "Controls (1/2)".
Set PageName/PageOrder on the modules to "Controls (1/2)"/0,
"Navigation"/10 and "Extra / presets"/30.
Fill Navigation.Controls and Navigation.Extra with the corresponding scripts
on that quest. The modules must share the core quest for this fixture's
RegisterModule call. Generate SEQ files if required by your quest setup.

Compile our PSC files with the installed game's Papyrus compiler and the Skyrim,
SKSE, SkyUI, MCM Helper, nl_mcm and JContainers SDK imports. Compile the Classic
parent before its child. Do not compile or ship the upstream SDK Guard stubs.
Install the real framework runtime scripts and dependencies.

Copy compiled PEX into each test mod's Scripts directory. Copy Helper/MCM into
MBLHelper's Data root. Copy translations as described below. Install all required
frameworks, enable the three test plugins, then start a new disposable game.
Nothing here is deployed to your active game automatically.

Custom content uses the real SkyUI splash SWF, not an invented placeholder.
SkyUI must provide Interface/skyui/skyui_splash.swf.
NL preset tests additionally require a working JContainers runtime.
These are framework-positive tests, not a no-SkyUI installation test.

## Coverage

| Family | Exercised features |
| --- | --- |
| Classic States / Names | Header, empty, text, toggle, slider, menu, color, input, keymap; state callbacks; duplicate states across pages |
| Classic Positional | All seven interactive families through numeric option callbacks; defaults, highlight, key conflicts, live SetOptionFlags |
| Classic behavior | Text cycler; static and dynamic menus; signed fractional slider, units; reset; disabled/hidden/unmap flags; both fill modes and cursor positioning |
| Classic presentation | HTML color, translated and missing keys, literal slash, empty page, custom SWF, confirmation/cancel |
| Classic lifecycle | Open/close/version trace; explicit delayed population and page-list rename controls via console functions |
| Helper Controls | All 12 schema control types, shortNames, dynamic SetMenuOptions, reset, help value substitution, local/global actions |
| Helper Sources | ModSetting bool/int/float/string, property bool/int/float/string, GlobalValue |
| Helper Conditions | Group controllers, hiddenToggle, AND/OR/ONLY/NOT and array conditions; disable/hide/skip |
| Helper lifecycle | OnSettingChange, OnPageSelect, RefreshMenu, dynamic menu update following an input edit |
| Helper hotkeys | Function, global function, control down/up event, harmless read-only console action; conflict policy |
| NL Controls | All seven interactive families using NL callback signatures, highlights, defaults and confirmation |
| NL Navigation | Shared state suffix IDs, same state on different modules, registration/removal, rename, order, forced navigation, landing page, close, opening hotkey |
| NL presentation | Paragraph wrapping/HTML, built-in font styles, paper/default switching, custom color, splash |
| NL Extra | SaveData/LoadData, save/load/list/delete fixture presets, opt-in persistent preset |
| Cross-framework | Logging of callbacks, persisted fixture values, empty/custom pages, classic-versus-Bridge comparison |

The Classic base and child share values deliberately: edit States and inspect
Positional. NL Controls and Extra do NOT share state values with Navigation:
a write to Shared___0 must never leak to the other module.

## Manual checks

1. First inspect every page in the original MCM and record expected values.
2. Inspect the same pages in Bridge, including names containing "/".
3. Change each control once. Check its trace and the value in the original MCM.
4. Drag a slider through many values and release once. Check committed writes
   and MCMMemory recording separately from page-open/metadata callback logs.
5. Disable and re-enable dependent controls. Their widgets AND reset buttons
   must follow the new state. Hidden and skipped controls must not become editable.
6. Cancel each dialog. Reset each control with a default. Test key unmapping,
   collisions, a mouse button and a gamepad button if available.
7. Edit Helper Caption and reopen its dynamic menu. The new caption must appear.
8. Rename/remove/re-register NL Extra and Controls. No ghost pages or identity
   collisions should remain. Test both direct opening and Journal redirection.
9. Save, change and load the NL counter preset. Persistent presets are opt-in
   and should only affect this test mod. Delete targets only the fixture preset.
10. Save/load the game and repeat. With MCMMemory present, record changes in
    both original MCM and Bridge and verify the same settings, without duplicates.

For delayed Classic content, run these console calls on the test quest:
`cqf MBLClassicQuest ExpandPages`, then
`cqf MBLClassicQuest PopulateLater`.
Open "Delayed content" immediately; it should become populated after the timer.
Call ExpandPages again to rename the last page. No timeout is injected by default.

To test INI precedence, copy Overrides/MBLHelper.ini to
Data/MCM/Settings/MBLHelper.ini before starting a test. The slider should initially
read 2.5 instead of the packaged default 1.25. This file is intentionally outside
the install tree so it cannot silently overwrite an existing user configuration.

## Localization

Localization/strings.tsv is ASCII with a tab separator. Convert it to UTF-16LE
with BOM when packaging as Interface/Translations/<plugin>_<language>.txt.
Use plugin names MBLClassic, MBLHelper and MBLNL and languages english/german.
The test translations are intentionally English in both language files.
The missing key has no translation by design. Do not strip its dollar prefix.

## Boundaries still requiring dedicated fixtures

- Interactive custom SWF widgets: only real splash rendering/fallback is tested.
- NL modules supplied by a separate plugin: this kit uses one core quest.
- Version upgrades: increment GetVersion in a second build to trigger upgrades.
- Stress limits, corrupted metadata, missing requirements and deliberately hung
  callbacks: these need isolated negative tests, not the normal feature fixture.
- Every skin, localization language, runtime and combination of dependencies.

## Verification and references

Run `powershell -NoProfile -File ./Validate.ps1` for structural checks.
This is not a Papyrus compiler or an in-game test runner.

Contracts were checked against the local SkyUI and MCM Helper reference sources
and https://github.com/MrOctopus/nl_mcm (SDK and examples, 2026-09-09).
The fixture scripts are original test code; no upstream runtime scripts are bundled.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$fixtureRoot = $PSScriptRoot
$files = Get-ChildItem -LiteralPath $fixtureRoot -Recurse -File
foreach ($file in $files) {
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    if ($bytes | Where-Object { $_ -gt 127 }) {
        throw "Non-ASCII content: $($file.FullName)"
    }
    $text = [System.IO.File]::ReadAllText($file.FullName)
    if ($text -match '(?m)[ \t]+\r?$') {
        throw "Trailing whitespace: $($file.FullName)"
    }
    if ($file.Extension -eq '.json') {
        $null = $text | ConvertFrom-Json
    }
}

$config = Get-Content -Raw -LiteralPath (Join-Path $fixtureRoot 'Helper/MCM/Config/MBLHelper/config.json') | ConvertFrom-Json
$allControls = @($config.content) + @($config.pages | ForEach-Object { $_.content })
$expectedTypes = @('empty', 'header', 'text', 'toggle', 'hiddenToggle', 'slider', 'stepper', 'menu', 'enum', 'color', 'keymap', 'input')
foreach ($type in $expectedTypes) {
    if ($type -notin $allControls.type) { throw "Missing Helper control: $type" }
}
$allowedKeys = @('type', 'id', 'text', 'help', 'position', 'valueOptions', 'action', 'groupCondition', 'groupBehavior', 'groupControl', 'ignoreConflicts', 'formatString')
foreach ($control in $allControls) {
    if ($null -eq $control) { continue }
    foreach ($key in $control.PSObject.Properties.Name) {
        if ($key -notin $allowedKeys) { throw "Unsupported control key: $key" }
    }
    if ($control.type -eq 'slider') {
        $v = $control.valueOptions
        if ($v.min -ge $v.max -or $v.step -le 0) { throw "Invalid slider: $($control.id)" }
    }
    if ($control.valueOptions.shortNames) {
        if ($control.valueOptions.shortNames.Count -ne $control.valueOptions.options.Count) {
            throw "Mismatched short names: $($control.id)"
        }
    }
}
foreach ($source in @('ModSettingBool', 'ModSettingInt', 'ModSettingFloat', 'PropertyValueBool', 'PropertyValueInt', 'PropertyValueFloat', 'GlobalValue')) {
    if ($source -notin $allControls.valueOptions.sourceType) { throw "Missing value source: $source" }
}
$keys = Get-Content -Raw -LiteralPath (Join-Path $fixtureRoot 'Helper/MCM/Config/MBLHelper/keybinds.json') | ConvertFrom-Json
foreach ($kind in @('CallFunction', 'CallGlobalFunction', 'SendEvent', 'RunConsoleCommand')) {
    if ($kind -notin $keys.keybinds.action.type) { throw "Missing key action: $kind" }
}
foreach ($key in $allControls | Where-Object { $_.type -eq 'keymap' }) {
    if ($key.id -notin $keys.keybinds.id) { throw "Undefined hotkey: $($key.id)" }
}
foreach ($script in $files | Where-Object { $_.Extension -eq '.psc' }) {
    $text = Get-Content -Raw -LiteralPath $script.FullName
    if ($text -notmatch ('(?im)^Scriptname ' + [regex]::Escape($script.BaseName) + '\b')) {
        throw "Script name mismatch: $($script.Name)"
    }
    foreach ($pair in @(@('event', 'endEvent'), @('state', 'endState'), @('function', 'endFunction'))) {
        $startPattern = '(?im)^\s*(?:(?:int|bool|float|string)\s+)?' + $pair[0] + '\s+'
        $endPattern = '(?im)^\s*' + $pair[1] + '\s*$'
        if ([regex]::Matches($text, $startPattern).Count -ne [regex]::Matches($text, $endPattern).Count) {
            throw "Unbalanced $($pair[0]): $($script.Name)"
        }
    }
}
Write-Output "Feature Lab structural checks passed: $($files.Count) files, 12 Helper control types, 7 numeric sources, 4 hotkey action types."
Write-Output 'Papyrus compilation and in-game behavior have not been verified by this checker.'

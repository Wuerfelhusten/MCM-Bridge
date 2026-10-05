Scriptname MBLClassic extends MBLClassicStates

int ToggleId
int SliderId
int MenuId
int ColorId
int KeyId
int InputId
int CycleId
bool Expanded
bool Populated

function ExpandPages()
    Expanded = !Expanded
    Pages = new string[7]
    Pages[0] = "States"
    Pages[1] = "Positional"
    Pages[2] = "Names (1/2)"
    Pages[3] = "Empty"
    Pages[4] = "Custom content"
    Pages[5] = "Delayed content"
    Pages[6] = "Added / page"
    if !Expanded
        Pages[6] = "Renamed / page"
    endif
    Debug.Trace("[FeatureLab] Classic.PagesChanged")
endFunction

function PopulateLater()
    Populated = false
    RegisterForSingleUpdate(2.0)
endFunction

event OnUpdate()
    Populated = true
    Debug.Trace("[FeatureLab] Classic.Populated")
    ForcePageReset()
endEvent

event OnPageReset(string page)
    if page == "Delayed content" && !Populated
        return
    endif
    if page != "Positional"
        parent.OnPageReset(page)
        return
    endif
    UnloadCustomContent()
    SetCursorFillMode(LEFT_TO_RIGHT)
    AddHeaderOption("Positional callbacks")
    AddHeaderOption("Second column")
    ToggleId = AddToggleOption("Toggle", Enabled)
    SliderId = AddSliderOption("Slider", Amount, "{2} s")
    MenuId = AddMenuOption("Menu", Choices[Choice])
    ColorId = AddColorOption("Color", Tint)
    KeyId = AddKeyMapOption("Key", KeyCode, OPTION_FLAG_WITH_UNMAP)
    InputId = AddInputOption("Input", Caption)
    CycleId = AddTextOption("Cycler", Choices[Choice])
    AddEmptyOption()
endEvent

event OnOptionSelect(int option)
    if option == ToggleId
        Enabled = !Enabled
        SetToggleOptionValue(option, Enabled)
        int flags = OPTION_FLAG_NONE
        if !Enabled
            flags = OPTION_FLAG_DISABLED
        endif
        SetOptionFlags(SliderId, flags)
    elseif option == CycleId
        Choice = (Choice + 1) % Choices.Length
        SetTextOptionValue(option, Choices[Choice])
    endif
    Debug.Trace("[FeatureLab] Classic.Positional.Select=" + option)
endEvent

event OnOptionSliderOpen(int option)
    SetSliderDialogStartValue(Amount)
    SetSliderDialogDefaultValue(1.25)
    SetSliderDialogRange(-5.0, 10.0)
    SetSliderDialogInterval(0.25)
endEvent

event OnOptionSliderAccept(int option, float value)
    Amount = value
    SetSliderOptionValue(option, Amount, "{2} s")
    Debug.Trace("[FeatureLab] Classic.Positional.Slider=" + value)
endEvent

event OnOptionMenuOpen(int option)
    SetMenuDialogOptions(Choices)
    SetMenuDialogStartIndex(Choice)
    SetMenuDialogDefaultIndex(1)
endEvent

event OnOptionMenuAccept(int option, int value)
    if value >= 0 && value < Choices.Length
        Choice = value
        SetMenuOptionValue(option, Choices[Choice])
    endif
    Debug.Trace("[FeatureLab] Classic.Positional.Menu=" + value)
endEvent

event OnOptionColorOpen(int option)
    SetColorDialogStartColor(Tint)
    SetColorDialogDefaultColor(0x33AAFF)
endEvent

event OnOptionColorAccept(int option, int value)
    Tint = value
    SetColorOptionValue(option, Tint)
    Debug.Trace("[FeatureLab] Classic.Positional.Color=" + value)
endEvent

event OnOptionInputOpen(int option)
    SetInputDialogStartText(Caption)
endEvent

event OnOptionInputAccept(int option, string value)
    Caption = value
    SetInputOptionValue(option, Caption)
    Debug.Trace("[FeatureLab] Classic.Positional.Input=" + value)
endEvent

event OnOptionKeyMapChange(int option, int value, string conflictControl, string conflictName)
    if conflictControl != ""
        if !ShowMessage("Conflict: " + conflictControl + " / " + conflictName)
            return
        endif
    endif
    KeyCode = value
    SetKeyMapOptionValue(option, KeyCode)
    Debug.Trace("[FeatureLab] Classic.Positional.Key=" + value)
endEvent

event OnOptionHighlight(int option)
    SetInfoText("Positional option " + option + ". Defaults and live flags must work.")
endEvent

event OnOptionDefault(int option)
    if option == ToggleId
        Enabled = true
    elseif option == SliderId
        Amount = 1.25
    elseif option == MenuId || option == CycleId
        Choice = 1
    elseif option == ColorId
        Tint = 0x33AAFF
    elseif option == KeyId
        KeyCode = -1
    elseif option == InputId
        Caption = "Initial / text"
    endif
    ForcePageReset()
    Debug.Trace("[FeatureLab] Classic.Positional.Default=" + option)
endEvent

string function GetCustomControl(int key)
    if key >= 0 && key == KeyCode
        return "Feature Lab key"
    endif
    return ""
endFunction

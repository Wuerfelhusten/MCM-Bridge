Scriptname MBLClassicStates extends SKI_ConfigBase

bool property Enabled = true auto
float property Amount = 1.25 auto
int property Choice = 1 auto
int property Tint = 0x33AAFF auto
int property KeyCode = -1 auto
string property Caption = "Initial / text" auto
string DynamicValue = "Not selected"
string[] property Choices auto hidden

function InitializeChoices()
    Choices = new string[3]
    Choices[0] = "First"
    Choices[1] = "Second"
    Choices[2] = "Third / literal"
endFunction

event OnConfigInit()
    ModName = "Feature Lab Classic"
    InitializeChoices()
    Pages = new string[5]
    Pages[0] = "States"
    Pages[1] = "Positional"
    Pages[2] = "Names (1/2)"
    Pages[3] = "Empty"
    Pages[4] = "Custom content"
endEvent

event OnPageReset(string page)
    UnloadCustomContent()
    if page == "Empty"
        return
    elseif page == "Custom content"
        LoadCustomContent("skyui/skyui_splash.swf", 0.0, 0.0)
        return
    endif
    int flags = OPTION_FLAG_NONE
    if !Enabled
        flags = OPTION_FLAG_DISABLED
    endif
    SetCursorFillMode(TOP_TO_BOTTOM)
    AddHeaderOption("<font color='#33AAFF'>All controls</font>")
    AddToggleOptionST("Toggle", "Enable dependent controls", Enabled)
    AddSliderOptionST("Slider", "Signed fractional slider", Amount, "{2} s", flags)
    AddMenuOptionST("Menu", "Static menu", Choices[Choice], flags)
    AddColorOptionST("Color", "RGB color", Tint, flags)
    AddInputOptionST("Input", "Text input", Caption, flags)
    AddEmptyOption()
    AddToggleOptionST("Hidden", "Hidden must not render", false, OPTION_FLAG_HIDDEN)
    SetCursorPosition(1)
    AddHeaderOption("Other column")
    AddKeyMapOptionST("Key", "Key with unmap and conflicts", KeyCode, OPTION_FLAG_WITH_UNMAP)
    AddTextOptionST("Cycle", "Text cycler", Choices[Choice])
    AddMenuOptionST("Dynamic", "Dynamic menu", DynamicValue)
    AddTextOptionST("Message", "Confirmation dialog", "Open")
    AddTextOptionST("ReadOnly", "Disabled text", "Not interactive", OPTION_FLAG_DISABLED)
    AddTextOptionST("MissingTranslation", "$MBL_MISSING_KEY", "Raw key is expected")
    AddTextOptionST("Translated", "$MBL_LABEL", "$MBL_VALUE")
endEvent

event OnConfigOpen()
    Debug.Trace("[FeatureLab] Classic.Open")
endEvent

event OnConfigClose()
    Debug.Trace("[FeatureLab] Classic.Close")
endEvent

int function GetVersion()
    return 1
endFunction

event OnVersionUpdate(int version)
    Debug.Trace("[FeatureLab] Classic.Version=" + version)
endEvent

state Toggle
    event OnSelectST()
        Enabled = !Enabled
        SetToggleOptionValueST(Enabled)
        ForcePageReset()
        Debug.Trace("[FeatureLab] Classic.Toggle.OnSelectST")
    endEvent

    event OnDefaultST()
        Enabled = true
        ForcePageReset()
        Debug.Trace("[FeatureLab] Classic.Toggle.OnDefaultST")
    endEvent
    event OnHighlightST()
        SetInfoText("Feature Lab: Toggle. Check callback logging and reset.")
    endEvent
endState

state Slider
    event OnSliderOpenST()
        SetSliderDialogStartValue(Amount)
        SetSliderDialogDefaultValue(1.25)
        SetSliderDialogRange(-5.0, 10.0)
        SetSliderDialogInterval(0.25)
        Debug.Trace("[FeatureLab] Classic.Slider.OnSliderOpenST")
    endEvent

    event OnSliderAcceptST(float value)
        Amount = value
        SetSliderOptionValueST(Amount, "{2} s")
        Debug.Trace("[FeatureLab] Classic.Slider.OnSliderAcceptST")
    endEvent

    event OnDefaultST()
        Amount = 1.25
        SetSliderOptionValueST(Amount, "{2} s")
        Debug.Trace("[FeatureLab] Classic.Slider.OnDefaultST")
    endEvent
    event OnHighlightST()
        SetInfoText("Feature Lab: Slider. Check callback logging and reset.")
    endEvent
endState

state Menu
    event OnMenuOpenST()
        SetMenuDialogOptions(Choices)
        SetMenuDialogStartIndex(Choice)
        SetMenuDialogDefaultIndex(1)
        Debug.Trace("[FeatureLab] Classic.Menu.OnMenuOpenST")
    endEvent

    event OnMenuAcceptST(int value)
        if value >= 0 && value < Choices.Length
            Choice = value
            SetMenuOptionValueST(Choices[Choice])
        endif
        Debug.Trace("[FeatureLab] Classic.Menu.OnMenuAcceptST")
    endEvent

    event OnDefaultST()
        Choice = 1
        SetMenuOptionValueST(Choices[Choice])
        Debug.Trace("[FeatureLab] Classic.Menu.OnDefaultST")
    endEvent
    event OnHighlightST()
        SetInfoText("Feature Lab: Menu. Check callback logging and reset.")
    endEvent
endState

state Color
    event OnColorOpenST()
        SetColorDialogStartColor(Tint)
        SetColorDialogDefaultColor(0x33AAFF)
        Debug.Trace("[FeatureLab] Classic.Color.OnColorOpenST")
    endEvent

    event OnColorAcceptST(int value)
        Tint = value
        SetColorOptionValueST(Tint)
        Debug.Trace("[FeatureLab] Classic.Color.OnColorAcceptST")
    endEvent

    event OnDefaultST()
        Tint = 0x33AAFF
        SetColorOptionValueST(Tint)
        Debug.Trace("[FeatureLab] Classic.Color.OnDefaultST")
    endEvent
    event OnHighlightST()
        SetInfoText("Feature Lab: Color. Check callback logging and reset.")
    endEvent
endState

state Input
    event OnInputOpenST()
        SetInputDialogStartText(Caption)
        Debug.Trace("[FeatureLab] Classic.Input.OnInputOpenST")
    endEvent

    event OnInputAcceptST(string value)
        Caption = value
        SetInputOptionValueST(Caption)
        Debug.Trace("[FeatureLab] Classic.Input.OnInputAcceptST")
    endEvent

    event OnDefaultST()
        Caption = "Initial / text"
        SetInputOptionValueST(Caption)
        Debug.Trace("[FeatureLab] Classic.Input.OnDefaultST")
    endEvent
    event OnHighlightST()
        SetInfoText("Feature Lab: Input. Check callback logging and reset.")
    endEvent
endState

state Key
    event OnKeyMapChangeST(int value, string conflictControl, string conflictName)
        if conflictControl != ""
            if !ShowMessage("Conflict: " + conflictControl + " / " + conflictName)
                return
            endif
        endif
        KeyCode = value
        SetKeyMapOptionValueST(KeyCode)
        Debug.Trace("[FeatureLab] Classic.Key.OnKeyMapChangeST")
    endEvent

    event OnDefaultST()
        KeyCode = -1
        SetKeyMapOptionValueST(KeyCode)
        Debug.Trace("[FeatureLab] Classic.Key.OnDefaultST")
    endEvent
    event OnHighlightST()
        SetInfoText("Feature Lab: Key. Check callback logging and reset.")
    endEvent
endState

state Cycle
    event OnSelectST()
        Choice = (Choice + 1) % Choices.Length
        SetTextOptionValueST(Choices[Choice])
        Debug.Trace("[FeatureLab] Classic.Cycle.OnSelectST")
    endEvent

    event OnDefaultST()
        Choice = 1
        SetTextOptionValueST(Choices[Choice])
        Debug.Trace("[FeatureLab] Classic.Cycle.OnDefaultST")
    endEvent
    event OnHighlightST()
        SetInfoText("Feature Lab: Cycle. Check callback logging and reset.")
    endEvent
endState

state Dynamic
    event OnMenuOpenST()
        string[] items = new string[2]
        items[0] = Caption
        items[1] = "Choice " + Choice
        SetMenuDialogOptions(items)
        SetMenuDialogStartIndex(0)
        SetMenuDialogDefaultIndex(0)
        Debug.Trace("[FeatureLab] Classic.Dynamic.OnMenuOpenST")
    endEvent

    event OnMenuAcceptST(int value)
        if value == 0
            DynamicValue = Caption
        else
            DynamicValue = "Choice " + Choice
        endif
        SetMenuOptionValueST(DynamicValue)
        Debug.Trace("[FeatureLab] Classic.Dynamic.OnMenuAcceptST")
    endEvent
    event OnHighlightST()
        SetInfoText("Feature Lab: Dynamic. Check callback logging and reset.")
    endEvent
endState

state Message
    event OnSelectST()
        bool accepted = ShowMessage("Feature Lab confirmation", true, "Confirm", "Cancel")
        Debug.Trace("[FeatureLab] confirmation=" + accepted)
        Debug.Trace("[FeatureLab] Classic.Message.OnSelectST")
    endEvent
    event OnHighlightST()
        SetInfoText("Feature Lab: Message. Check callback logging and reset.")
    endEvent
endState

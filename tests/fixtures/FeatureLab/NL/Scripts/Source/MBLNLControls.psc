Scriptname MBLNLControls extends nl_mcm_module

bool property Enabled = true auto
float property Amount = 1.25 auto
int property Choice = 1 auto
int property Tint = 0x33AAFF auto
int property KeyCode = -1 auto
string property Caption = "Initial / text" auto
string DynamicValue = "Not selected"
string[] Choices

function InitializeChoices()
    Choices = new string[3]
    Choices[0] = "First"
    Choices[1] = "Second"
    Choices[2] = "Third / literal"
endFunction

event OnPageInit()
    InitializeChoices()
endEvent

event OnPageDraw()
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

event OnConfigClose()
    Debug.Trace("[FeatureLab] NL.Close")
endEvent

event OnGameReload()
    Debug.Trace("[FeatureLab] NL.Reload")
endEvent

int function GetVersion()
    return 1
endFunction

event OnVersionUpdate(int version)
    Debug.Trace("[FeatureLab] NL.Version=" + version)
endEvent

state Toggle
    event OnSelectST(string state_id)
        Enabled = !Enabled
        SetToggleOptionValueST(Enabled)
        ForcePageReset()
        Debug.Trace("[FeatureLab] NL.Toggle.OnSelectST")
    endEvent

    event OnDefaultST(string state_id)
        Enabled = true
        ForcePageReset()
        Debug.Trace("[FeatureLab] NL.Toggle.OnDefaultST")
    endEvent
    event OnHighlightST(string state_id)
        SetInfoText("Feature Lab: Toggle. Check callback logging and reset.")
    endEvent
endState

state Slider
    event OnSliderOpenST(string state_id)
        SetSliderDialogStartValue(Amount)
        SetSliderDialogDefaultValue(1.25)
        SetSliderDialogRange(-5.0, 10.0)
        SetSliderDialogInterval(0.25)
        Debug.Trace("[FeatureLab] NL.Slider.OnSliderOpenST")
    endEvent

    event OnSliderAcceptST(string state_id, float value)
        Amount = value
        SetSliderOptionValueST(Amount, "{2} s")
        Debug.Trace("[FeatureLab] NL.Slider.OnSliderAcceptST")
    endEvent

    event OnDefaultST(string state_id)
        Amount = 1.25
        SetSliderOptionValueST(Amount, "{2} s")
        Debug.Trace("[FeatureLab] NL.Slider.OnDefaultST")
    endEvent
    event OnHighlightST(string state_id)
        SetInfoText("Feature Lab: Slider. Check callback logging and reset.")
    endEvent
endState

state Menu
    event OnMenuOpenST(string state_id)
        SetMenuDialogOptions(Choices)
        SetMenuDialogStartIndex(Choice)
        SetMenuDialogDefaultIndex(1)
        Debug.Trace("[FeatureLab] NL.Menu.OnMenuOpenST")
    endEvent

    event OnMenuAcceptST(string state_id, int value)
        if value >= 0 && value < Choices.Length
            Choice = value
            SetMenuOptionValueST(Choices[Choice])
        endif
        Debug.Trace("[FeatureLab] NL.Menu.OnMenuAcceptST")
    endEvent

    event OnDefaultST(string state_id)
        Choice = 1
        SetMenuOptionValueST(Choices[Choice])
        Debug.Trace("[FeatureLab] NL.Menu.OnDefaultST")
    endEvent
    event OnHighlightST(string state_id)
        SetInfoText("Feature Lab: Menu. Check callback logging and reset.")
    endEvent
endState

state Color
    event OnColorOpenST(string state_id)
        SetColorDialogStartColor(Tint)
        SetColorDialogDefaultColor(0x33AAFF)
        Debug.Trace("[FeatureLab] NL.Color.OnColorOpenST")
    endEvent

    event OnColorAcceptST(string state_id, int value)
        Tint = value
        SetColorOptionValueST(Tint)
        Debug.Trace("[FeatureLab] NL.Color.OnColorAcceptST")
    endEvent

    event OnDefaultST(string state_id)
        Tint = 0x33AAFF
        SetColorOptionValueST(Tint)
        Debug.Trace("[FeatureLab] NL.Color.OnDefaultST")
    endEvent
    event OnHighlightST(string state_id)
        SetInfoText("Feature Lab: Color. Check callback logging and reset.")
    endEvent
endState

state Input
    event OnInputOpenST(string state_id)
        SetInputDialogStartText(Caption)
        Debug.Trace("[FeatureLab] NL.Input.OnInputOpenST")
    endEvent

    event OnInputAcceptST(string state_id, string value)
        Caption = value
        SetInputOptionValueST(Caption)
        Debug.Trace("[FeatureLab] NL.Input.OnInputAcceptST")
    endEvent

    event OnDefaultST(string state_id)
        Caption = "Initial / text"
        SetInputOptionValueST(Caption)
        Debug.Trace("[FeatureLab] NL.Input.OnDefaultST")
    endEvent
    event OnHighlightST(string state_id)
        SetInfoText("Feature Lab: Input. Check callback logging and reset.")
    endEvent
endState

state Key
    event OnKeyMapChangeST(string state_id, int value)
        KeyCode = value
        SetKeyMapOptionValueST(KeyCode)
        Debug.Trace("[FeatureLab] NL.Key.OnKeyMapChangeST")
    endEvent

    event OnDefaultST(string state_id)
        KeyCode = -1
        SetKeyMapOptionValueST(KeyCode)
        Debug.Trace("[FeatureLab] NL.Key.OnDefaultST")
    endEvent
    event OnHighlightST(string state_id)
        SetInfoText("Feature Lab: Key. Check callback logging and reset.")
    endEvent
endState

state Cycle
    event OnSelectST(string state_id)
        Choice = (Choice + 1) % Choices.Length
        SetTextOptionValueST(Choices[Choice])
        Debug.Trace("[FeatureLab] NL.Cycle.OnSelectST")
    endEvent

    event OnDefaultST(string state_id)
        Choice = 1
        SetTextOptionValueST(Choices[Choice])
        Debug.Trace("[FeatureLab] NL.Cycle.OnDefaultST")
    endEvent
    event OnHighlightST(string state_id)
        SetInfoText("Feature Lab: Cycle. Check callback logging and reset.")
    endEvent
endState

state Dynamic
    event OnMenuOpenST(string state_id)
        string[] items = new string[2]
        items[0] = Caption
        items[1] = "Choice " + Choice
        SetMenuDialogOptions(items)
        SetMenuDialogStartIndex(0)
        SetMenuDialogDefaultIndex(0)
        Debug.Trace("[FeatureLab] NL.Dynamic.OnMenuOpenST")
    endEvent

    event OnMenuAcceptST(string state_id, int value)
        if value == 0
            DynamicValue = Caption
        else
            DynamicValue = "Choice " + Choice
        endif
        SetMenuOptionValueST(DynamicValue)
        Debug.Trace("[FeatureLab] NL.Dynamic.OnMenuAcceptST")
    endEvent
    event OnHighlightST(string state_id)
        SetInfoText("Feature Lab: Dynamic. Check callback logging and reset.")
    endEvent
endState

state Message
    event OnSelectST(string state_id)
        bool accepted = ShowMessage("Feature Lab confirmation", true, "Confirm", "Cancel")
        Debug.Trace("[FeatureLab] confirmation=" + accepted)
        Debug.Trace("[FeatureLab] NL.Message.OnSelectST")
    endEvent
    event OnHighlightST(string state_id)
        SetInfoText("Feature Lab: Message. Check callback logging and reset.")
    endEvent
endState

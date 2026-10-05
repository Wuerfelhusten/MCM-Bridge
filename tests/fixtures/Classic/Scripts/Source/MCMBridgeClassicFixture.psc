scriptName MCMBridgeClassicFixture extends SKI_ConfigBase

bool property ToggleValue = true auto
float property SliderValue = 5.0 auto
int property MenuValue = 1 auto
string[] property MenuOptions auto

event OnConfigInit()
    ModName = "MCM Bridge Classic Fixture"
    Pages = new string[1]
    Pages[0] = "General"
    MenuOptions = new string[3]
    MenuOptions[0] = "One"
    MenuOptions[1] = "Two"
    MenuOptions[2] = "Three"
endEvent

event OnPageReset(string page)
    SetCursorFillMode(TOP_TO_BOTTOM)
    AddHeaderOption("Fixture")
    AddToggleOptionST("toggle", "Toggle", ToggleValue)
    AddSliderOptionST("slider", "Slider", SliderValue, "{0}")
    AddMenuOptionST("menu", "Menu", MenuOptions[MenuValue])
    AddToggleOption("Disabled", false, OPTION_FLAG_DISABLED)
endEvent

state toggle
    event OnSelectST()
        ToggleValue = !ToggleValue
        SetToggleOptionValueST(ToggleValue)
        Debug.Trace("MCMBridgeFixture Toggle callback")
    endEvent
endState

state slider
    event OnSliderOpenST()
        SetSliderDialogStartValue(SliderValue)
        SetSliderDialogDefaultValue(5.0)
        SetSliderDialogRange(0.0, 10.0)
        SetSliderDialogInterval(0.5)
    endEvent

    event OnSliderAcceptST(float value)
        SliderValue = value
        SetSliderOptionValueST(value, "{0}")
        Debug.Trace("MCMBridgeFixture Slider callback")
    endEvent
endState

state menu
    event OnMenuOpenST()
        SetMenuDialogOptions(MenuOptions)
        SetMenuDialogStartIndex(MenuValue)
        SetMenuDialogDefaultIndex(1)
    endEvent

    event OnMenuAcceptST(int index)
        MenuValue = index
        SetMenuOptionValueST(MenuOptions[index])
        Debug.Trace("MCMBridgeFixture Menu callback")
    endEvent
endState

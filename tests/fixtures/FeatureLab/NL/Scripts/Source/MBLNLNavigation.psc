Scriptname MBLNLNavigation extends nl_mcm_module

MBLNLControls property Controls auto
MBLNLExtra property Extra auto
bool Renamed
bool ShowExtra = true
bool[] SharedValues

event OnPageInit()
    SharedValues = new bool[2]
endEvent

event OnPageDraw()
    SetCursorFillMode(TOP_TO_BOTTOM)
    AddHeaderOption(FONT_PRIMARY("Modules and navigation"))
    AddParagraph("First line\nSecond line with <b>bold</b> and <i>italic</i> text.", FONT_INFO())
    AddTextOptionST("Rename", "Rename controls page", "Toggle")
    AddToggleOptionST("Register", "Extra module registered", ShowExtra)
    AddTextOptionST("Navigate", "Go to controls page", "Open")
    AddTextOptionST("Landing", "Use controls as landing page", "Set")
    AddTextOptionST("Close", "Close MCM and Journal", "Close")
    AddKeyMapOptionST("Hotkey", "Open MCM hotkey", QuickHotkey, OPTION_FLAG_WITH_UNMAP)
    SetCursorPosition(1)
    AddHeaderOption(FONT_SECONDARY("Shared states and presentation"))
    AddToggleOptionST("Shared___0", "Shared state A", SharedValues[0])
    AddToggleOptionST("Shared___1", "Shared state B", SharedValues[1])
    AddTextOptionST("Font", "Default / paper font", "Switch")
    AddTextOptionST("Colors", FONT_SUCCESS("Success"), FONT_DANGER("Danger"))
    AddTextOptionST("Colors2", FONT_WARNING("Warning"), FONT_CUSTOM("Custom", "#FF00FF"))
    AddTextOptionST("Order", "Move controls page", "Toggle order")
    AddTextOptionST("Splash", "Use SkyUI splash", "Set")
endEvent

state Shared
    event OnSelectST(string state_id)
        int index = state_id as int
        SharedValues[index] = !SharedValues[index]
        SetToggleOptionValueST(SharedValues[index], false, "Shared___" + state_id)
        Debug.Trace("[FeatureLab] NL.Shared=" + state_id)
    endEvent

    event OnDefaultST(string state_id)
        int index = state_id as int
        SharedValues[index] = false
        SetToggleOptionValueST(false, false, "Shared___" + state_id)
    endEvent
endState

state Rename
    event OnSelectST(string state_id)
        Renamed = !Renamed
        string name = "Controls (1/2)"
        if Renamed
            name = "Renamed / controls"
        endif
        Debug.Trace("[FeatureLab] NL.Rename=" + Controls.RenameModule(name))
        ForcePageListReset()
    endEvent
endState

state Register
    event OnSelectST(string state_id)
        if ShowExtra
            Debug.Trace("[FeatureLab] NL.Unregister=" + Extra.UnregisterModule())
        else
            Debug.Trace("[FeatureLab] NL.Register=" + Extra.RegisterModule("Extra / presets", 30))
        endif
        ShowExtra = Extra.IsModuleRegistered
        ForcePageListReset()
    endEvent
endState

state Navigate
    event OnSelectST(string state_id)
        GoToPage(Controls.PageName)
    endEvent
endState

state Landing
    event OnSelectST(string state_id)
        SetLandingPage(Controls.PageName)
    endEvent
endState

state Close
    event OnSelectST(string state_id)
        CloseMCM(true)
    endEvent
endState

state Hotkey
    event OnKeyMapChangeST(string state_id, int keyCode)
        QuickHotkey = keyCode
        SetKeyMapOptionValueST(keyCode)
    endEvent
endState

state Font
    event OnSelectST(string state_id)
        if CurrentFont == FONT_TYPE_DEFAULT
            SetFont(FONT_TYPE_PAPER)
        else
            SetFont(FONT_TYPE_DEFAULT)
        endif
        ForcePageReset()
    endEvent
endState

state Order
    event OnSelectST(string state_id)
        if Controls.PageOrder == 0
            Controls.PageOrder = 25
        else
            Controls.PageOrder = 0
        endif
        ForcePageListReset()
    endEvent
endState

state Splash
    event OnSelectST(string state_id)
        SetSplashScreen("skyui/skyui_splash.swf")
    endEvent
endState

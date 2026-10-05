Scriptname MBLHelper extends MCM_ConfigBase

bool property BoolValue = true auto
int property IntValue = 2 auto
float property FloatValue = 1.25 auto
string property Caption = "Initial / text" auto
string property MenuValue = "Beta long" auto
string property DynamicValue = "First" auto
string property ActionLabel = "Run" auto

event OnConfigInit()
    ModName = "MBLHelper"
endEvent

event OnConfigOpen()
    UpdateOptions()
    Debug.Trace("[FeatureLab] Helper.Open")
endEvent

event OnPageSelect(string page)
    UpdateOptions()
    Debug.Trace("[FeatureLab] Helper.Page=" + page)
endEvent

event OnSettingChange(string id)
    Debug.Trace("[FeatureLab] Helper.Change=" + id)
    if id == "Caption"
        UpdateOptions()
        RefreshMenu()
    endif
endEvent

event OnConfigClose()
    Debug.Trace("[FeatureLab] Helper.Close")
endEvent

function UpdateOptions()
    string[] options = new string[3]
    options[0] = "First"
    options[1] = Caption
    options[2] = "Last / literal"
    SetMenuOptions("DynamicMenu", options)
endFunction

function ToggleHiddenGroup()
    SetModSettingBool("bHidden:Lab", !GetModSettingBool("bHidden:Lab"))
    RefreshMenu()
endFunction

function LogAction(string origin, bool enabled, int count, float amount)
    Debug.Trace("[FeatureLab] Helper.Action=" + origin + "," + enabled + "," + count + "," + amount)
endFunction

event OnControlDown(string control)
    Debug.Trace("[FeatureLab] Helper.ControlDown=" + control)
endEvent

event OnControlUp(string control, float holdTime)
    Debug.Trace("[FeatureLab] Helper.ControlUp=" + control + "," + holdTime)
endEvent

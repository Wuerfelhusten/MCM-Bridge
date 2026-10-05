Scriptname MBLNLExtra extends nl_mcm_module

int Counter
bool Persistent

event OnPageDraw()
    SetCursorFillMode(TOP_TO_BOTTOM)
    AddHeaderOption("Presets require JContainers")
    AddTextOptionST("Counter", "Stored counter", Counter as string)
    AddTextOptionST("Save", "Save fixture preset", "Save")
    AddTextOptionST("Load", "Load fixture preset", "Load")
    AddTextOptionST("Delete", "Delete fixture preset only", "Delete")
    AddTextOptionST("List", "List presets in log", "List")
    AddToggleOptionST("Persistent", "Persistent fixture preset", Persistent)
    AddToggleOptionST("Shared___0", "Same state ID as another page", Counter != 0)
endEvent

int function SaveData()
    int data = JMap.object()
    JMap.setInt(data, "version", 1)
    JMap.setInt(data, "counter", Counter)
    Debug.Trace("[FeatureLab] NL.SaveData=" + Counter)
    return data
endFunction

function LoadData(int data)
    if JMap.getInt(data, "version") != 1
        return
    endif
    Counter = JMap.getInt(data, "counter")
    Debug.Trace("[FeatureLab] NL.LoadData=" + Counter)
endFunction

state Counter
    event OnSelectST(string state_id)
        Counter += 1
        SetTextOptionValueST(Counter as string)
    endEvent
endState

state Save
    event OnSelectST(string state_id)
        SaveMCMToPreset("MCMBridgeFeatureLab/manual")
    endEvent
endState

state Load
    event OnSelectST(string state_id)
        LoadMCMFromPreset("MCMBridgeFeatureLab/manual")
        ForcePageReset()
    endEvent
endState

state Delete
    event OnSelectST(string state_id)
        if ShowMessage("Delete the Feature Lab manual preset?")
            DeleteMCMSavedPreset("MCMBridgeFeatureLab/manual")
        endif
    endEvent
endState

state List
    event OnSelectST(string state_id)
        string[] names = GetMCMSavedPresets("None", "MCMBridgeFeatureLab")
        int i = 0
        while i < names.Length
            Debug.Trace("[FeatureLab] NL.Preset=" + names[i])
            i += 1
        endWhile
    endEvent
endState

state Persistent
    event OnSelectST(string state_id)
        Persistent = !Persistent
        if Persistent
            SetPersistentMCMPreset("MCMBridgeFeatureLab/persistent")
        else
            SetPersistentMCMPreset("")
        endif
        SetToggleOptionValueST(Persistent)
    endEvent
endState

state Shared
    event OnSelectST(string state_id)
        if Counter == 0
            Counter = 1
        else
            Counter = 0
        endif
        SetToggleOptionValueST(Counter != 0, false, "Shared___" + state_id)
        Debug.Trace("[FeatureLab] NL.Extra.Shared=" + state_id)
    endEvent
endState

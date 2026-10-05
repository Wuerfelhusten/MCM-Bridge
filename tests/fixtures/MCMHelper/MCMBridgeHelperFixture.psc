scriptName MCMBridgeHelperFixture extends MCM_ConfigBase

string property DynamicMode = "One" auto

event OnSettingChange(string setting, string value)
    Debug.Trace("MCMBridgeFixture Helper callback " + setting + "=" + value)
endEvent

function ApplyDynamicMode(string value)
    DynamicMode = value
    Debug.Trace("MCMBridgeFixture Dynamic menu callback " + value)
endFunction

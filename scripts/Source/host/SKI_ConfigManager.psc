Scriptname SKI_ConfigManager extends SKI_QuestBase Hidden

String Property JOURNAL_MENU = "Journal Menu" AutoReadOnly
String Property MENU_ROOT = "_root.ConfigPanelFader.configPanel" AutoReadOnly
; Native admission reads backing storage without invoking a Papyrus getter.
Int Property MCMBridgeManagerVersion = 1 Auto Hidden
SKI_ConfigBase[] _modConfigs
String[] _modNames
Int _curConfigID = 0
Int _configCount = 0
SKI_ConfigBase _activeConfig
Bool _lockInit = false
Bool _locked = false
Bool _cleanupFlag = false
Int _addCounter = 0
Int _updateCounter = 0
Int _bridgeMigrationVersion = 0

Event OnInit()
	_modConfigs = new SKI_ConfigBase[1]
	_modNames = new String[1]
	OnGameReload()
EndEvent

Event OnGameReload()
	; Existing saves retain registrations made by the original manager.
	UnregisterForAllModEvents()
	UnregisterForAllMenus()
	UnregisterForUpdate()
	GoToState("")
	_activeConfig = none
	_updateCounter = 0
	OnUpdate()
EndEvent

Int Function BridgeCallAdmissionContract()
	return 2
EndFunction

Event OnUpdate()
	if MCMBridgeNative.GetProtocolVersion() != BridgeCallAdmissionContract()
		Log("Native host protocol is missing or incompatible; bootstrap was not started")
		return
	endIf
	Int result = BridgeAwait(MCMBridgeRegistry.Bootstrap(self))
	if result == 1
		if _bridgeMigrationVersion < 1
			; Native import retains saved slots. Only obsolete manager locks are migrated.
			_lockInit = false
			_locked = false
			_cleanupFlag = false
			_addCounter = 0
			_bridgeMigrationVersion = 1
		endIf
		SendModEvent("SKICP_configManagerReady")
	elseIf result == -2 && _updateCounter < 30
		; Only bootstrap retries are timed. Registration itself is event-driven.
		_updateCounter += 1
		RegisterForSingleUpdate(1)
	else
		Log("Native host bootstrap failed; MCM execution remains inactive")
	endIf
EndEvent

Int Function GetVersion()
	return 4
EndFunction

Int Function RegisterMod(SKI_ConfigBase a_menu, String a_modName)
	return BridgeAwait(MCMBridgeRegistry.Register(self, a_menu, a_modName))
EndFunction

Int Function UnregisterMod(SKI_ConfigBase a_menu)
	return BridgeAwait(MCMBridgeRegistry.Unregister(self, a_menu))
EndFunction

Function ForceReset()
	if BridgeAwait(MCMBridgeRegistry.Reset(self)) == 1
		SendModEvent("SKICP_configManagerReset")
		SendModEvent("SKICP_configManagerReady")
	else
		Log("Native registry reset rejected while the host is unavailable or busy")
	endIf
EndFunction

Function CleanUp()
	BridgeAwait(MCMBridgeRegistry.Bootstrap(self))
EndFunction

Int Function BridgeAwait(Int a_request)
	if a_request == -2
		return -2
	endIf
	if a_request <= 0
		return -1
	endIf
	Int attempts = 0
	Int result = MCMBridgeRegistry.TakeResult(a_request)
	while result == -3 && attempts < 1000
		; No native stack is suspended across a save. Missing requests fail closed.
		Utility.WaitMenuMode(0.01)
		attempts += 1
		result = MCMBridgeRegistry.TakeResult(a_request)
	endWhile
	if result == -3
		MCMBridgeRegistry.Cancel(a_request)
		return -1
	endIf
	return result
EndFunction

Int Function NextID()
	; The native registry allocates identities without reusing removed slots.
	return _modConfigs.Length
EndFunction

Function Log(String a_msg)
	MCMBridgeNative.LogError(a_msg)
EndFunction

; Saved Journal events must not start a second configuration session.
Event OnMenuOpen(String a_menuName)
EndEvent
Event OnMenuClose(String a_menuName)
EndEvent
Event OnModSelect(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnPageSelect(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnOptionHighlight(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnOptionSelect(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnOptionDefault(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnKeymapChange(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnSliderSelect(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnSliderAccept(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnMenuSelect(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnMenuAccept(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnColorSelect(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnColorAccept(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnInputSelect(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnInputAccept(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent
Event OnDialogCancel(String a_eventName, String a_strArg, Float a_numArg, Form a_sender)
EndEvent

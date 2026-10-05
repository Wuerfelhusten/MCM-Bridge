Scriptname SKI_ConfigBase extends SKI_QuestBase

; Compatibility declarations follow the SkyUI MCM script contract.
; This facade requires a controller-owned native token before executing menus.
string property JOURNAL_MENU = "Journal Menu" autoReadonly
string property MENU_ROOT = "_root.ConfigPanelFader.configPanel" autoReadonly
int property STATE_DEFAULT = 0 autoReadonly
int property STATE_RESET = 1 autoReadonly
int property STATE_SLIDER = 2 autoReadonly
int property STATE_MENU = 3 autoReadonly
int property STATE_COLOR = 4 autoReadonly
int property STATE_INPUT = 5 autoReadonly
int property OPTION_TYPE_EMPTY = 0x00 autoReadonly
int property OPTION_TYPE_HEADER = 0x01 autoReadonly
int property OPTION_TYPE_TEXT = 0x02 autoReadonly
int property OPTION_TYPE_TOGGLE = 0x03 autoReadonly
int property OPTION_TYPE_SLIDER = 0x04 autoReadonly
int property OPTION_TYPE_MENU = 0x05 autoReadonly
int property OPTION_TYPE_COLOR = 0x06 autoReadonly
int property OPTION_TYPE_KEYMAP = 0x07 autoReadonly
int property OPTION_TYPE_INPUT = 0x08 autoReadonly
int property OPTION_FLAG_NONE = 0x00 autoReadonly
int property OPTION_FLAG_DISABLED = 0x01 autoReadonly
int property OPTION_FLAG_HIDDEN = 0x02 autoReadonly
int property OPTION_FLAG_WITH_UNMAP = 0x04 autoReadonly
int property LEFT_TO_RIGHT = 1 autoReadonly
int property TOP_TO_BOTTOM = 2 autoReadonly
SKI_ConfigManager _configManager
bool _initialized = false
int _configID = -1
string _currentPage = ""
int _currentPageNum = 0
int _state = 0
int _cursorPosition = 0
int _cursorFillMode = 1
int[] _optionFlagsBuf
string[] _textBuf
string[] _strValueBuf
float[] _numValueBuf
float[] _sliderParams
int[] _menuParams
int[] _colorParams
int _activeOption = -1
string _infoText
string _inputStartText
bool _messageResult = false
bool _waitForMessage = false
string[] _stateOptionMap

String Property ModName Auto
String[] Property Pages Auto
String Property CurrentPage
	String Function Get()
		return _currentPage
	EndFunction
EndProperty
Int _bridgeToken
Int _bridgeDialog
Bool _bridgeRegistering
; Native admission reads backing storage without invoking a Papyrus getter.
Int Property MCMBridgeFacadeVersion = 1 Auto Hidden

Bool Function BindNativeHost(Int a_token)
	if MCMBridgeNative.GetProtocolVersion() != 2
		return false
	endIf
	if !MCMBridgeNative.IsActive(a_token)
		return false
	endIf
	if _bridgeToken != a_token
		_waitForMessage = false
		_messageResult = false
	endIf
	_bridgeToken = a_token
	return true
EndFunction

Int Function GetNativeHostProtocol()
	return 1
EndFunction

Bool Function BridgeCheckOption(Int a_slot, Int a_type)
	if !MCMBridgeNative.IsActive(_bridgeToken) || a_slot < 0 || a_slot >= _optionFlagsBuf.Length
		return false
	endIf
	return (_optionFlagsBuf[a_slot] % 256) == a_type
EndFunction

Bool Function BridgeCanSelectOption(Int a_slot)
	if !MCMBridgeNative.IsActive(_bridgeToken) || a_slot < 0 || a_slot >= _optionFlagsBuf.Length
		return false
	endIf
	int packed = _optionFlagsBuf[a_slot]
	int optionType = packed % 256
	; Disabled rows must not reach mod callbacks, including direct host callers.
	return (optionType == 2 || optionType == 3) && Math.LogicalAnd(packed, 0x300) == 0
EndFunction

Int Function BridgeAddOption(Int a_type, String a_label, String a_text, Float a_number, Int a_flags, String a_state)
	if _state != STATE_RESET || !MCMBridgeNative.SetCursor(_bridgeToken, _cursorPosition, _cursorFillMode)
		return -1
	endIf
	int option = MCMBridgeNative.AddOption(_bridgeToken, a_type, a_label, a_text, a_number, a_flags, a_state)
	if option < 0
		return -1
	endIf
	int slot = option % 256
	_optionFlagsBuf[slot] = a_type + a_flags * 256
	_textBuf[slot] = a_label
	_strValueBuf[slot] = a_text
	_numValueBuf[slot] = a_number
	if a_state != ""
		_stateOptionMap[slot] = a_state
	endIf
	_cursorPosition = slot + _cursorFillMode
	if _cursorPosition >= 128
		_cursorPosition = -1
	endIf
	return option
EndFunction

event OnInit()
	OnGameReload()
EndEvent

event OnGameReload()
	_bridgeRegistering = false
	if !_initialized
		_initialized = true
		_sliderParams = new float[5]
		_menuParams = new int[2]
		_colorParams = new int[2]
		OnConfigInit()
	endIf
	RegisterForModEvent("SKICP_configManagerReady", "OnConfigManagerReady")
	RegisterForModEvent("SKICP_configManagerReset", "OnConfigManagerReset")
	CheckVersion()
	; Registration must not keep the initialization stack waiting on another script.
	; A per-form event also recovers a manager readiness event missed by slow initialization.
	string registrationEvent = "MCMBridge_register_" + GetFormID()
	RegisterForModEvent(registrationEvent, "OnBridgeRegistrationRequested")
	SendModEvent(registrationEvent)
EndEvent

event OnBridgeRegistrationRequested(string a_eventName, string a_strArg, float a_numArg, Form a_sender)
	if a_sender != self
		return
	endIf
	UnregisterForModEvent(a_eventName)
	SKI_ConfigManager manager = Game.GetFormFromFile(0x802, "SkyUI_SE.esp") as SKI_ConfigManager
	if manager != none
		OnConfigManagerReady("SKICP_configManagerReady", "", 0, manager)
	endIf
EndEvent

event OnConfigInit()
EndEvent

event OnConfigRegister()
EndEvent

event OnConfigOpen()
EndEvent

event OnConfigClose()
EndEvent

event OnVersionUpdate(int a_version)
EndEvent

event OnPageReset(string a_page)
EndEvent

event OnOptionHighlight(int a_option)
EndEvent

event OnOptionSelect(int a_option)
EndEvent

event OnOptionDefault(int a_option)
EndEvent

event OnOptionSliderOpen(int a_option)
EndEvent

event OnOptionSliderAccept(int a_option, float a_value)
EndEvent

event OnOptionMenuOpen(int a_option)
EndEvent

event OnOptionMenuAccept(int a_option, int a_index)
EndEvent

event OnOptionColorOpen(int a_option)
EndEvent

event OnOptionColorAccept(int a_option, int a_color)
EndEvent

event OnOptionInputOpen(int a_option)
EndEvent

event OnOptionInputAccept(int a_option, string a_input)
EndEvent

event OnOptionKeyMapChange(int a_option, int a_keyCode, string a_conflictControl, string a_conflictName)
EndEvent

event OnHighlightST()
EndEvent

event OnSelectST()
EndEvent

event OnDefaultST()
EndEvent

event OnSliderOpenST()
EndEvent

event OnSliderAcceptST(float a_value)
EndEvent

event OnMenuOpenST()
EndEvent

event OnMenuAcceptST(int a_index)
EndEvent

event OnColorOpenST()
EndEvent

event OnColorAcceptST(int a_color)
EndEvent

event OnInputOpenST()
EndEvent

event OnInputAcceptST(string a_input)
EndEvent

event OnKeyMapChangeST(int a_keyCode, string a_conflictControl, string a_conflictName)
EndEvent

event OnConfigManagerReset(string a_eventName, string a_strArg, float a_numArg, Form a_sender)
	_configManager = none
EndEvent

event OnConfigManagerReady(string a_eventName, string a_strArg, float a_numArg, Form a_sender)
	SKI_ConfigManager manager = a_sender as SKI_ConfigManager
	if manager == none || manager == _configManager || _bridgeRegistering
		return
	endIf
	_bridgeRegistering = true
	int slot = manager.RegisterMod(self, ModName)
	_bridgeRegistering = false
	if slot >= 0
		_configID = slot
		_configManager = manager
		OnConfigRegister()
	endIf
EndEvent

event OnMessageDialogClose(string a_eventName, string a_strArg, float a_numArg, Form a_sender)
	; Saved legacy events cannot complete a native message request.
EndEvent

int function GetVersion()
	return 1
EndFunction

string function GetCustomControl(int a_keyCode)
	return ""
EndFunction

function ForcePageReset()
	MCMBridgeNative.SetPresentation(_bridgeToken, 4, "")
EndFunction

function SetTitleText(string a_text)
	MCMBridgeNative.SetPresentation(_bridgeToken, 0, a_text)
EndFunction

function SetInfoText(string a_text)
	_infoText = a_text
	MCMBridgeNative.SetPresentation(_bridgeToken, 1, a_text)
EndFunction

function SetCursorPosition(int a_position)
	if a_position >= -1 && a_position < 128
		_cursorPosition = a_position
	endIf
EndFunction

function SetCursorFillMode(int a_fillMode)
	if a_fillMode == 1 || a_fillMode == 2
		_cursorFillMode = a_fillMode
	endIf
EndFunction

int function AddEmptyOption()
	return AddOption(0, "", "", 0, 0)
EndFunction

int function AddHeaderOption(string a_text, int a_flags = 0)
	return AddOption(1, a_text, "", 0, a_flags)
EndFunction

int function AddTextOption(string a_text, string a_value, int a_flags = 0)
	return AddOption(2, a_text, a_value, 0, a_flags)
EndFunction

int function AddToggleOption(string a_text, bool a_checked, int a_flags = 0)
	return AddOption(3, a_text, "", a_checked as int, a_flags)
EndFunction

int function AddSliderOption(string a_text, float a_value, string a_formatString = "{0}", int a_flags = 0)
	return AddOption(4, a_text, a_formatString, a_value, a_flags)
EndFunction

int function AddMenuOption(string a_text, string a_value, int a_flags = 0)
	return AddOption(5, a_text, a_value, 0, a_flags)
EndFunction

int function AddColorOption(string a_text, int a_color, int a_flags = 0)
	return AddOption(6, a_text, "", a_color, a_flags)
EndFunction

int function AddKeyMapOption(string a_text, int a_keyCode, int a_flags = 0)
	return AddOption(7, a_text, "", a_keyCode, a_flags)
EndFunction

int function AddInputOption(string a_text, string a_value, int a_flags = 0)
	return AddOption(8, a_text, a_value, 0, a_flags)
EndFunction

function AddTextOptionST(string a_stateName, string a_text, string a_value, int a_flags = 0)
	AddOptionST(a_stateName, 2, a_text, a_value, 0, a_flags)
EndFunction

function AddToggleOptionST(string a_stateName, string a_text, bool a_checked, int a_flags = 0)
	AddOptionST(a_stateName, 3, a_text, "", a_checked as int, a_flags)
EndFunction

function AddSliderOptionST(string a_stateName, string a_text, float a_value, string a_formatString = "{0}", int a_flags = 0)
	AddOptionST(a_stateName, 4, a_text, a_formatString, a_value, a_flags)
EndFunction

function AddMenuOptionST(string a_stateName, string a_text, string a_value, int a_flags = 0)
	AddOptionST(a_stateName, 5, a_text, a_value, 0, a_flags)
EndFunction

function AddColorOptionST(string a_stateName, string a_text, int a_color, int a_flags = 0)
	AddOptionST(a_stateName, 6, a_text, "", a_color, a_flags)
EndFunction

function AddKeyMapOptionST(string a_stateName, string a_text, int a_keyCode, int a_flags = 0)
	AddOptionST(a_stateName, 7, a_text, "", a_keyCode, a_flags)
EndFunction

function AddInputOptionST(string a_stateName, string a_text, string a_value, int a_flags = 0)
	AddOptionST(a_stateName, 8, a_text, a_value, 0, a_flags)
EndFunction

function LoadCustomContent(string a_source, float a_x = 0.0, float a_y = 0.0)
	MCMBridgeNative.SetCustomContent(_bridgeToken, a_source, a_x, a_y)
EndFunction

function UnloadCustomContent()
	MCMBridgeNative.SetPresentation(_bridgeToken, 3, "")
EndFunction

function SetOptionFlags(int a_option, int a_flags, bool a_noUpdate = false)
	int slot = a_option % 256
	if _state == STATE_RESET || slot < 0 || slot >= _optionFlagsBuf.Length
		return
	endIf
	if MCMBridgeNative.SetFlags(_bridgeToken, slot + _currentPageNum * 256, a_flags)
		_optionFlagsBuf[slot] = (_optionFlagsBuf[slot] % 256) + a_flags * 256
		if !a_noUpdate
			MCMBridgeNative.PublishPage(_bridgeToken)
		endIf
	endIf
EndFunction

function SetTextOptionValue(int a_option, string a_value, bool a_noUpdate = false)
	int slot = a_option % 256
	if !BridgeCheckOption(slot, 2)
		return
	endIf
	SetOptionStrValue(slot, a_value, a_noUpdate)
EndFunction

function SetToggleOptionValue(int a_option, bool a_checked, bool a_noUpdate = false)
	int slot = a_option % 256
	if !BridgeCheckOption(slot, 3)
		return
	endIf
	SetOptionNumValue(slot, a_checked as int, a_noUpdate)
EndFunction

function SetSliderOptionValue(int a_option, float a_value, string a_formatString = "{0}", bool a_noUpdate = false)
	int slot = a_option % 256
	if !BridgeCheckOption(slot, 4)
		return
	endIf
	SetOptionValues(slot, a_formatString, a_value, a_noUpdate)
EndFunction

function SetMenuOptionValue(int a_option, string a_value, bool a_noUpdate = false)
	int slot = a_option % 256
	if !BridgeCheckOption(slot, 5)
		return
	endIf
	SetOptionStrValue(slot, a_value, a_noUpdate)
EndFunction

function SetColorOptionValue(int a_option, int a_color, bool a_noUpdate = false)
	int slot = a_option % 256
	if !BridgeCheckOption(slot, 6)
		return
	endIf
	SetOptionNumValue(slot, a_color, a_noUpdate)
EndFunction

function SetKeyMapOptionValue(int a_option, int a_keyCode, bool a_noUpdate = false)
	int slot = a_option % 256
	if !BridgeCheckOption(slot, 7)
		return
	endIf
	SetOptionNumValue(slot, a_keyCode, a_noUpdate)
EndFunction

function SetInputOptionValue(int a_option, string a_value, bool a_noUpdate = false)
	int slot = a_option % 256
	if !BridgeCheckOption(slot, 8)
		return
	endIf
	SetOptionStrValue(slot, a_value, a_noUpdate)
EndFunction

function SetOptionFlagsST(int a_flags, bool a_noUpdate = false, string a_stateName = "")
	int slot = GetStateOptionIndex(a_stateName)
	if slot >= 0
		SetOptionFlags(slot, a_flags, a_noUpdate)
	endIf
EndFunction

function SetTextOptionValueST(string a_value, bool a_noUpdate = false, string a_stateName = "")
	int slot = GetStateOptionIndex(a_stateName)
	if slot >= 0
		SetTextOptionValue(slot, a_value, a_noUpdate)
	endIf
EndFunction

function SetToggleOptionValueST(bool a_checked, bool a_noUpdate = false, string a_stateName = "")
	int slot = GetStateOptionIndex(a_stateName)
	if slot >= 0
		SetToggleOptionValue(slot, a_checked, a_noUpdate)
	endIf
EndFunction

function SetSliderOptionValueST(float a_value, string a_formatString = "{0}", bool a_noUpdate = false, string a_stateName = "")
	int slot = GetStateOptionIndex(a_stateName)
	if slot >= 0
		SetSliderOptionValue(slot, a_value, a_formatString, a_noUpdate)
	endIf
EndFunction

function SetMenuOptionValueST(string a_value, bool a_noUpdate = false, string a_stateName = "")
	int slot = GetStateOptionIndex(a_stateName)
	if slot >= 0
		SetMenuOptionValue(slot, a_value, a_noUpdate)
	endIf
EndFunction

function SetColorOptionValueST(int a_color, bool a_noUpdate = false, string a_stateName = "")
	int slot = GetStateOptionIndex(a_stateName)
	if slot >= 0
		SetColorOptionValue(slot, a_color, a_noUpdate)
	endIf
EndFunction

function SetKeyMapOptionValueST(int a_keyCode, bool a_noUpdate = false, string a_stateName = "")
	int slot = GetStateOptionIndex(a_stateName)
	if slot >= 0
		SetKeyMapOptionValue(slot, a_keyCode, a_noUpdate)
	endIf
EndFunction

function SetInputOptionValueST(string a_value, bool a_noUpdate = false, string a_stateName = "")
	int slot = GetStateOptionIndex(a_stateName)
	if slot >= 0
		SetInputOptionValue(slot, a_value, a_noUpdate)
	endIf
EndFunction

function SetSliderDialogStartValue(float a_value)
	if _state == STATE_SLIDER
		_sliderParams[0] = a_value
		MCMBridgeNative.SetSliderParameter(_bridgeDialog, 0, a_value)
	endIf
EndFunction

function SetSliderDialogDefaultValue(float a_value)
	if _state == STATE_SLIDER
		_sliderParams[1] = a_value
		MCMBridgeNative.SetSliderParameter(_bridgeDialog, 1, a_value)
	endIf
EndFunction

function SetSliderDialogRange(float a_minValue, float a_maxValue)
	if _state == STATE_SLIDER
		_sliderParams[2] = a_minValue
		_sliderParams[3] = a_maxValue
		MCMBridgeNative.SetSliderParameter(_bridgeDialog, 2, a_minValue)
		MCMBridgeNative.SetSliderParameter(_bridgeDialog, 3, a_maxValue)
	endIf
EndFunction

function SetSliderDialogInterval(float a_value)
	if _state == STATE_SLIDER
		_sliderParams[4] = a_value
		MCMBridgeNative.SetSliderParameter(_bridgeDialog, 4, a_value)
	endIf
EndFunction

function SetMenuDialogStartIndex(int a_value)
	if _state == STATE_MENU
		_menuParams[0] = a_value
		MCMBridgeNative.SetDialogIndex(_bridgeDialog, 5, 0, a_value)
	endIf
EndFunction

function SetMenuDialogDefaultIndex(int a_value)
	if _state == STATE_MENU
		_menuParams[1] = a_value
		MCMBridgeNative.SetDialogIndex(_bridgeDialog, 5, 1, a_value)
	endIf
EndFunction

function SetMenuDialogOptions(string[] a_options)
	if _state == STATE_MENU
		MCMBridgeNative.SetDialogOptions(_bridgeDialog, a_options)
	endIf
EndFunction

function SetColorDialogStartColor(int a_color)
	if _state == STATE_COLOR
		_colorParams[0] = a_color
		MCMBridgeNative.SetDialogIndex(_bridgeDialog, 6, 0, a_color)
	endIf
EndFunction

function SetColorDialogDefaultColor(int a_color)
	if _state == STATE_COLOR
		_colorParams[1] = a_color
		MCMBridgeNative.SetDialogIndex(_bridgeDialog, 6, 1, a_color)
	endIf
EndFunction

function SetInputDialogStartText(string a_text)
	if _state == STATE_INPUT
		_inputStartText = a_text
		MCMBridgeNative.SetDialogInput(_bridgeDialog, a_text)
	endIf
EndFunction

bool function ShowMessage(string a_message, bool a_withCancel = true, string a_acceptLabel = "$Accept", string a_cancelLabel = "$Cancel")
	if _waitForMessage || !MCMBridgeNative.IsActive(_bridgeToken)
		return false
	endIf
	_waitForMessage = true
	_messageResult = false
	if !a_withCancel
		a_cancelLabel = ""
	endIf
	int request = MCMBridgeNative.RequestMessage(_bridgeToken, a_message, a_acceptLabel, a_cancelLabel)
	int result = MCMBridgeNative.TakeMessage(_bridgeToken, request)
	while result == -1 && MCMBridgeNative.IsActive(_bridgeToken)
		Utility.WaitMenuMode(0.1)
		result = MCMBridgeNative.TakeMessage(_bridgeToken, request)
	endWhile
	_messageResult = result == 1
	_waitForMessage = false
	return _messageResult
EndFunction

function Error(string a_msg)
	MCMBridgeNative.LogError(a_msg)
EndFunction


Int Function BridgeAwaitCall(Int a_request)
	if a_request <= 0
		return -1
	endIf
	int result = MCMBridgeNative.TakeCall(a_request)
	int attempts = 0
	while result == -3 && attempts < 1000
		Utility.WaitMenuMode(0.01)
		result = MCMBridgeNative.TakeCall(a_request)
		attempts += 1
	endWhile
	return result
EndFunction

Int Function BridgeEnterCall()
	int request = MCMBridgeNative.EnterCall()
	if request == -1
		return -1
	endIf
	int token = BridgeAwaitCall(request)
	if token <= 0 || !BindNativeHost(token)
		Error("Native host call admission failed; no callback executed")
		return 0
	endIf
	return request
EndFunction

Function BridgeLeaveCall(Int a_request, Bool a_closed)
	if a_request <= 0
		return
	endIf
	bool valid = true
	if !a_closed
		valid = BridgePublishBuffers()
	endIf
	if BridgeAwaitCall(MCMBridgeNative.LeaveCall(a_request, valid)) != 1
		Error("Native host call completion failed; callback must not be retried")
	endIf
EndFunction

function OpenConfig()
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	if !BindNativeHost(MCMBridgeNative.AcquireConfig())
		Error("Native host ownership could not be acquired; config was not opened")
		BridgeLeaveCall(admission, false)
		return
	endIf
	SetPage("", -1)
	OnConfigOpen()
	BridgeLeaveCall(admission, false)
EndFunction

function CloseConfig()
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	if !MCMBridgeNative.IsActive(_bridgeToken)
		BridgeLeaveCall(admission, true)
		return
	endIf
	OnConfigClose()
	_waitForMessage = false
	_activeOption = -1
	_bridgeDialog = 0
	_state = STATE_DEFAULT
	_optionFlagsBuf = new int[1]
	_textBuf = new string[1]
	_strValueBuf = new string[1]
	_numValueBuf = new float[1]
	_stateOptionMap = new string[1]
	BridgeLeaveCall(admission, true)
EndFunction

function SetPage(string a_page, int a_index)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	; Admission retains the original caller until the page and buffers are complete.
	if !MCMBridgeNative.AdmitPage(_bridgeToken, a_page, a_index)
		BridgeLeaveCall(admission, false)
		return
	endIf
	_currentPage = a_page
	_currentPageNum = a_index + 1
	ClearOptionBuffers()
	if a_page == ""
		SetTitleText(ModName)
	else
		SetTitleText(a_page)
	endIf
	_state = STATE_RESET
	OnPageReset(a_page)
	_state = STATE_DEFAULT
	WriteOptionBuffers()
	BridgeLeaveCall(admission, false)
EndFunction

int function AddOption(int a_optionType, string a_text, string a_strValue, float a_numValue, int a_flags)
	return BridgeAddOption(a_optionType, a_text, a_strValue, a_numValue, a_flags, "")
EndFunction

function AddOptionST(string a_stateName, int a_optionType, string a_text, string a_strValue, float a_numValue, int a_flags)
	BridgeAddOption(a_optionType, a_text, a_strValue, a_numValue, a_flags, a_stateName)
EndFunction

int function GetStateOptionIndex(string a_stateName)
	if a_stateName == ""
		a_stateName = GetState()
	endIf
	if a_stateName == ""
		return -1
	endIf
	return _stateOptionMap.Find(a_stateName)
EndFunction

function WriteOptionBuffers()
	BridgePublishBuffers()
EndFunction

Bool Function BridgePublishBuffers()
	; Helper writes these mirrors directly. Reconcile once at the callback boundary.
	if MCMBridgeNative.ImportBuffers(_bridgeToken, _optionFlagsBuf, _textBuf, _strValueBuf, _numValueBuf, _stateOptionMap)
		return MCMBridgeNative.PublishPage(_bridgeToken)
	endIf
	return false
EndFunction

function ClearOptionBuffers()
	_optionFlagsBuf = new int[128]
	_textBuf = new string[128]
	_strValueBuf = new string[128]
	_numValueBuf = new float[128]
	_stateOptionMap = new string[128]
	_cursorPosition = 0
	_cursorFillMode = LEFT_TO_RIGHT
	MCMBridgeNative.BeginPage(_bridgeToken, _currentPage, _currentPageNum - 1)
EndFunction

function SetOptionStrValue(int a_index, string a_strValue, bool a_noUpdate)
	if a_index >= 0 && a_index < _numValueBuf.Length
		SetOptionValues(a_index, a_strValue, _numValueBuf[a_index], a_noUpdate)
	endIf
EndFunction

function SetOptionNumValue(int a_index, float a_numValue, bool a_noUpdate)
	if a_index >= 0 && a_index < _strValueBuf.Length
		SetOptionValues(a_index, _strValueBuf[a_index], a_numValue, a_noUpdate)
	endIf
EndFunction

function SetOptionValues(int a_index, string a_strValue, float a_numValue, bool a_noUpdate)
	if _state == STATE_RESET || a_index < 0 || a_index >= _optionFlagsBuf.Length
		return
	endIf
	if MCMBridgeNative.SetValue(_bridgeToken, a_index + _currentPageNum * 256, a_strValue, a_numValue)
		_strValueBuf[a_index] = a_strValue
		_numValueBuf[a_index] = a_numValue
		if !a_noUpdate
			MCMBridgeNative.PublishPage(_bridgeToken)
		endIf
	endIf
EndFunction

function RequestSliderDialogData(int a_index)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	if !BridgeCheckOption(a_index, 4)
		BridgeLeaveCall(admission, false)
		return
	endIf
	_activeOption = a_index + _currentPageNum * 256
	_bridgeDialog = MCMBridgeNative.BeginDialog(_bridgeToken, _activeOption)
	if _bridgeDialog == 0
		_activeOption = -1
		BridgeLeaveCall(admission, false)
		return
	endIf
	_sliderParams = new float[5]
	_sliderParams[3] = 1
	_sliderParams[4] = 1
	_state = STATE_SLIDER
	string optionState = _stateOptionMap[a_index]
	if optionState != ""
		string previousState = GetState()
		GoToState(optionState)
		OnSliderOpenST()
		GoToState(previousState)
	else
		OnOptionSliderOpen(a_index + _currentPageNum * 256)
	endIf
	_state = STATE_DEFAULT
	int parameter = 0
	while parameter < 5
		MCMBridgeNative.SetSliderParameter(_bridgeDialog, parameter, _sliderParams[parameter])
		parameter += 1
	endWhile
	MCMBridgeNative.FinishDialog(_bridgeToken, _bridgeDialog)
	_bridgeDialog = 0
	BridgeLeaveCall(admission, false)
EndFunction

function RequestMenuDialogData(int a_index)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	if !BridgeCheckOption(a_index, 5)
		BridgeLeaveCall(admission, false)
		return
	endIf
	_activeOption = a_index + _currentPageNum * 256
	_bridgeDialog = MCMBridgeNative.BeginDialog(_bridgeToken, _activeOption)
	if _bridgeDialog == 0
		_activeOption = -1
		BridgeLeaveCall(admission, false)
		return
	endIf
	_menuParams = new int[2]
	_menuParams[0] = -1
	_menuParams[1] = -1
	_state = STATE_MENU
	string optionState = _stateOptionMap[a_index]
	if optionState != ""
		string previousState = GetState()
		GoToState(optionState)
		OnMenuOpenST()
		GoToState(previousState)
	else
		OnOptionMenuOpen(a_index + _currentPageNum * 256)
	endIf
	_state = STATE_DEFAULT
	; The adapter reads indices after completion; Helper can store float variants.
	MCMBridgeNative.FinishDialog(_bridgeToken, _bridgeDialog)
	_bridgeDialog = 0
	BridgeLeaveCall(admission, false)
EndFunction

function RequestColorDialogData(int a_index)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	if !BridgeCheckOption(a_index, 6)
		BridgeLeaveCall(admission, false)
		return
	endIf
	_activeOption = a_index + _currentPageNum * 256
	_bridgeDialog = MCMBridgeNative.BeginDialog(_bridgeToken, _activeOption)
	if _bridgeDialog == 0
		_activeOption = -1
		BridgeLeaveCall(admission, false)
		return
	endIf
	_colorParams = new int[2]
	_colorParams[0] = -1
	_colorParams[1] = -1
	_state = STATE_COLOR
	string optionState = _stateOptionMap[a_index]
	if optionState != ""
		string previousState = GetState()
		GoToState(optionState)
		OnColorOpenST()
		GoToState(previousState)
	else
		OnOptionColorOpen(a_index + _currentPageNum * 256)
	endIf
	_state = STATE_DEFAULT
	MCMBridgeNative.SetDialogIndex(_bridgeDialog, 6, 0, _colorParams[0])
	MCMBridgeNative.SetDialogIndex(_bridgeDialog, 6, 1, _colorParams[1])
	MCMBridgeNative.FinishDialog(_bridgeToken, _bridgeDialog)
	_bridgeDialog = 0
	BridgeLeaveCall(admission, false)
EndFunction

function RequestInputDialogData(int a_index)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	if !BridgeCheckOption(a_index, 8)
		BridgeLeaveCall(admission, false)
		return
	endIf
	_activeOption = a_index + _currentPageNum * 256
	_bridgeDialog = MCMBridgeNative.BeginDialog(_bridgeToken, _activeOption)
	if _bridgeDialog == 0
		_activeOption = -1
		BridgeLeaveCall(admission, false)
		return
	endIf
	_inputStartText = ""
	_state = STATE_INPUT
	string optionState = _stateOptionMap[a_index]
	if optionState != ""
		string previousState = GetState()
		GoToState(optionState)
		OnInputOpenST()
		GoToState(previousState)
	else
		OnOptionInputOpen(a_index + _currentPageNum * 256)
	endIf
	_state = STATE_DEFAULT
	MCMBridgeNative.SetDialogInput(_bridgeDialog, _inputStartText)
	MCMBridgeNative.FinishDialog(_bridgeToken, _bridgeDialog)
	_bridgeDialog = 0
	BridgeLeaveCall(admission, false)
EndFunction

function SetSliderValue(float a_value)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	int slot = _activeOption % 256
	if _activeOption < 0 || !BridgeCheckOption(slot, 4)
		BridgeLeaveCall(admission, false)
		return
	endIf
	string optionState = _stateOptionMap[slot]
	if optionState != ""
		string previousState = GetState()
		GoToState(optionState)
		OnSliderAcceptST(a_value)
		GoToState(previousState)
	else
		OnOptionSliderAccept(slot + _currentPageNum * 256, a_value)
	endIf
	_activeOption = -1
	BridgeLeaveCall(admission, false)
EndFunction

function SetMenuIndex(int a_index)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	int slot = _activeOption % 256
	if _activeOption < 0 || !BridgeCheckOption(slot, 5)
		BridgeLeaveCall(admission, false)
		return
	endIf
	string optionState = _stateOptionMap[slot]
	if optionState != ""
		string previousState = GetState()
		GoToState(optionState)
		OnMenuAcceptST(a_index)
		GoToState(previousState)
	else
		OnOptionMenuAccept(slot + _currentPageNum * 256, a_index)
	endIf
	_activeOption = -1
	BridgeLeaveCall(admission, false)
EndFunction

function SetColorValue(int a_color)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	int slot = _activeOption % 256
	if _activeOption < 0 || !BridgeCheckOption(slot, 6)
		BridgeLeaveCall(admission, false)
		return
	endIf
	string optionState = _stateOptionMap[slot]
	if optionState != ""
		string previousState = GetState()
		GoToState(optionState)
		OnColorAcceptST(a_color)
		GoToState(previousState)
	else
		OnOptionColorAccept(slot + _currentPageNum * 256, a_color)
	endIf
	_activeOption = -1
	BridgeLeaveCall(admission, false)
EndFunction

function SetInputText(string a_text)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	int slot = _activeOption % 256
	if _activeOption < 0 || !BridgeCheckOption(slot, 8)
		BridgeLeaveCall(admission, false)
		return
	endIf
	string optionState = _stateOptionMap[slot]
	if optionState != ""
		string previousState = GetState()
		GoToState(optionState)
		OnInputAcceptST(a_text)
		GoToState(previousState)
	else
		OnOptionInputAccept(slot + _currentPageNum * 256, a_text)
	endIf
	_activeOption = -1
	BridgeLeaveCall(admission, false)
EndFunction

function SelectOption(int a_index)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	if !BridgeCanSelectOption(a_index) || a_index >= _stateOptionMap.Length
		BridgeLeaveCall(admission, false)
		return
	endIf
	string optionState = _stateOptionMap[a_index]
	if optionState != ""
		string previousState = GetState()
		GoToState(optionState)
		OnSelectST()
		GoToState(previousState)
	else
		OnOptionSelect(a_index + _currentPageNum * 256)
	endIf
	BridgeLeaveCall(admission, false)
EndFunction

function ResetOption(int a_index)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	if !MCMBridgeNative.IsActive(_bridgeToken) || a_index < 0 || a_index >= _stateOptionMap.Length
		BridgeLeaveCall(admission, false)
		return
	endIf
	string optionState = _stateOptionMap[a_index]
	if optionState != ""
		string previousState = GetState()
		GoToState(optionState)
		OnDefaultST()
		GoToState(previousState)
	else
		OnOptionDefault(a_index + _currentPageNum * 256)
	endIf
	BridgeLeaveCall(admission, false)
EndFunction

function HighlightOption(int a_index)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	SetInfoText("")
	if MCMBridgeNative.IsActive(_bridgeToken) && a_index >= 0 && a_index < _stateOptionMap.Length
		string optionState = _stateOptionMap[a_index]
		if optionState != ""
			string previousState = GetState()
			GoToState(optionState)
			OnHighlightST()
			GoToState(previousState)
		else
			OnOptionHighlight(a_index + _currentPageNum * 256)
		endIf
	endIf
	MCMBridgeNative.SetPresentation(_bridgeToken, 1, _infoText)
	BridgeLeaveCall(admission, false)
EndFunction

function RemapKey(int a_index, int a_keyCode, string a_conflictControl, string a_conflictName)
	int admission = BridgeEnterCall()
	if admission == 0
		return
	endIf
	if !MCMBridgeNative.IsActive(_bridgeToken) || a_index < 0 || a_index >= _stateOptionMap.Length
		BridgeLeaveCall(admission, false)
		return
	endIf
	string optionState = _stateOptionMap[a_index]
	if optionState != ""
		string previousState = GetState()
		GoToState(optionState)
		OnKeyMapChangeST(a_keyCode, a_conflictControl, a_conflictName)
		GoToState(previousState)
	else
		OnOptionKeyMapChange(a_index + _currentPageNum * 256, a_keyCode, a_conflictControl, a_conflictName)
	endIf
	BridgeLeaveCall(admission, false)
EndFunction

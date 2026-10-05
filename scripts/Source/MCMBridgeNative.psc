Scriptname MCMBridgeNative Hidden Native

Int Function EnterCall() Global Native
Int Function TakeCall(Int a_request) Global Native
Int Function LeaveCall(Int a_request, Bool a_valid) Global Native

; Internal facade protocol. Tokens belong to the current host operation, not the save.
Int Function GetProtocolVersion() Global Native
Function LogError(String a_message) Global Native
Bool Function IsActive(Int a_token) Global Native
Int Function AcquireConfig() Global Native
Bool Function AdmitPage(Int a_token, String a_page, Int a_index) Global Native
Int Function BeginDialog(Int a_token, Int a_option) Global Native
Bool Function FinishDialog(Int a_token, Int a_request) Global Native
Bool Function SetPresentation(Int a_token, Int a_kind, String a_text) Global Native
Bool Function SetCustomContent(Int a_token, String a_source, Float a_x, Float a_y) Global Native
Bool Function ImportBuffers(Int a_token, Int[] a_flags, String[] a_labels, String[] a_strings, Float[] a_numbers, String[] a_states) Global Native
Int Function RequestMessage(Int a_token, String a_message, String a_accept, String a_cancel) Global Native
Int Function TakeMessage(Int a_token, Int a_request) Global Native
Bool Function BeginPage(Int a_token, String a_page, Int a_index) Global Native
Bool Function SetCursor(Int a_token, Int a_position, Int a_fillMode) Global Native
Int Function AddOption(Int a_token, Int a_type, String a_label, String a_text, Float a_value, Int a_flags, String a_state) Global Native
Bool Function SetValue(Int a_token, Int a_optionID, String a_text, Float a_value) Global Native
Bool Function SetFlags(Int a_token, Int a_optionID, Int a_flags) Global Native
Bool Function PublishPage(Int a_token) Global Native
Bool Function SetSliderParameter(Int a_request, Int a_index, Float a_value) Global Native
Bool Function SetDialogIndex(Int a_request, Int a_type, Int a_index, Int a_value) Global Native
Bool Function SetDialogOptions(Int a_request, String[] a_options) Global Native
Bool Function SetDialogInput(Int a_request, String a_text) Global Native

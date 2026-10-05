Scriptname SKI_QuestBase extends Quest Hidden

Int Property CurrentVersion Auto Hidden

Function CheckVersion()
	Int version = GetVersion()
	if CurrentVersion < version
		; Preserve the inherited callback order and commit only after both callbacks.
		OnVersionUpdateBase(version)
		OnVersionUpdate(version)
		CurrentVersion = version
	endIf
EndFunction

Int Function GetVersion()
	return 1
EndFunction

Event OnVersionUpdateBase(Int a_version)
EndEvent

Event OnVersionUpdate(Int a_version)
EndEvent

Event OnGameReload()
EndEvent

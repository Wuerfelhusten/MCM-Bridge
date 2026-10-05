Scriptname SKI_PlayerLoadGameAlias extends ReferenceAlias

Event OnPlayerLoadGame()
	SKI_QuestBase owner = GetOwningQuest() as SKI_QuestBase
	if owner != none
		owner.OnGameReload()
	endIf
EndEvent

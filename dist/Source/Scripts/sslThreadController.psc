scriptname sslThreadController extends sslThreadModel
{
	Controller script to recognize player actions (hotkey inputs etc) to manually interact with scene logic
}

; *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-* ;
; ----------------------------------------------------------------------------- ;
;        ██╗███╗   ██╗████████╗███████╗██████╗ ███╗   ██╗ █████╗ ██╗            ;
;        ██║████╗  ██║╚══██╔══╝██╔════╝██╔══██╗████╗  ██║██╔══██╗██║            ;
;        ██║██╔██╗ ██║   ██║   █████╗  ██████╔╝██╔██╗ ██║███████║██║            ;
;        ██║██║╚██╗██║   ██║   ██╔══╝  ██╔══██╗██║╚██╗██║██╔══██║██║            ;
;        ██║██║ ╚████║   ██║   ███████╗██║  ██║██║ ╚████║██║  ██║███████╗       ;
;        ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝  ╚═╝╚══════╝       ;
; ----------------------------------------------------------------------------- ;
; *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-* ;

Function EnableHotkeys(bool forced = false)
	If(!HasPlayer && !forced)
		return
	EndIf
	If (Config.UseSceneMenu)
		EnableMenuEvents()
	EndIf
	EnableTraditionalHotkeys()
EndFunction

Function DisableHotkeys()
	SexLabUtil.ToggleFreeCamera(0) ; If free cam is active here will glitch out controls?
	If (Config.UseSceneMenu)
		DisableMenuEvents()
	EndIf
	DisableTraditionalHotkeys()
EndFunction

; ------------------------------------------------------- ;
; --- Menu Events                                     --- ;
; ------------------------------------------------------- ;

String[] _MenuEvents
int _AutoAdvanceCache

Function EnableMenuEvents()
	If (!TryOpenSceneMenu())
		return
	EndIf
	_AutoAdvanceCache = -1
	_MenuEvents = new String[8]
	_MenuEvents[0] = "SL_AdvanceScene"
	_MenuEvents[1] = "SL_SetSpeed"
	_MenuEvents[2] = "SL_MoveScene"
	_MenuEvents[3] = "SL_EndScene"
	_MenuEvents[4] = "SL_SetAnnotations"
	_MenuEvents[5] = "SL_SetOffset"
	_MenuEvents[6] = "SL_StartAdjustOffset"
	_MenuEvents[7] = "SL_SetActiveScene"
	int i = 0
	While (i < _MenuEvents.Length)
		RegisterForModEvent(_MenuEvents[i], "MenuEvent")
		i += 1
	EndWhile
EndFunction

Function DisableMenuEvents()
	int i = 0
	While (i < _MenuEvents.Length)
		UnregisterForModEvent(_MenuEvents[i])
		i += 1
	EndWhile
	TryCloseSceneMenu()
EndFunction

Event MenuEvent(string asEventName, string asStringArg, float afNumArg, form akSender)
	Log("MenuEvent: " + asEventName)
	If (asEventName == "SL_SetActiveScene")
		PickRandomScene(asStringArg)
	ElseIf (asEventName == "SL_AdvanceScene")
		If (afNumArg)
			GoToStage(Stage - 1)
		Else
			PlayNextImpl(asStringArg)
		EndIf
	ElseIf (asEventName == "SL_SetSpeed")
		UpdateBaseSpeed(afNumArg)
		If (afNumArg == 0.0)
			_AutoAdvanceCache = AutoAdvance as int
			AutoAdvance = false
		ElseIf (_AutoAdvanceCache != -1)
			AutoAdvance = _AutoAdvanceCache as bool
			_AutoAdvanceCache = -1
		EndIf
	ElseIf (asEventName == "SL_MoveScene")
		MoveScene()
	ElseIf (asEventName == "SL_EndScene")
		EndAnimation()
	ElseIf (asEventName == "SL_SetAnnotations")
		UpdateAnnotations(asStringArg)
	ElseIf (asEventName == "SL_SetOffset")
		If (akSender == none)
			SetSceneOffset(afNumArg, asStringArg)
		ElseIf (akSender as Actor)
			SetStageOffset(akSender as Actor, afNumArg, asStringArg)
		Else
			Log("SetOffset: Sender is not an actor")
		EndIf
	ElseIf (asEventName == "SL_StartAdjustOffset")
		; TODO: impl
	EndIf
EndEvent

; ------------------------------------------------------- ;
; --- Executed Functions [Menu Events]                --- ;
; ------------------------------------------------------- ;

Message Property RepositionInfoMsg Auto
{[Ok, Cancel, Don't show again]}

Function PickRandomScene(String asNewScene)
	String[] sceneSet = GetPlayingScenes()
	If(sceneSet.Length < 2)
		Log("PickRandomScene: No other scenes to pick from")
		return
	EndIf
	UnregisterForUpdate()
	If (asNewScene == "")
		int i = sceneSet.Find(GetActiveScene())
		int r = Utility.RandomInt(0, sceneSet.Length - 1)
		While(r == i)
			r = Utility.RandomInt(0, sceneSet.Length - 1)
		EndWhile
		asNewScene = sceneSet[r]
	EndIf
	Log("Changing running scene from " + GetActiveScene() + " to " + asNewScene)
	SendThreadEvent("AnimationChange")
	ResetScene(asNewScene)
EndFunction

Function MoveScene()
	If (!SexLabRegistry.IsCompatibleCenter(GetActiveScene(), Game.GetPlayer()))
		Debug.Notification("This scene does not support repositioning")
		return
	EndIf
	UnregisterForUpdate()
	If (StorageUtil.GetIntValue(none, "SEXLAB_REPOSITIONMSG_INFO", 0) == 0)
		; "You have 30 secs to position yourself to a new center location.\nHold down the 'Move Scene' hotkey to relocate the center instantly to your current position"
		int choice = RepositionInfoMsg.Show()
		If (choice == 1)
			return
		ElseIf (choice == 2)
			StorageUtil.SetIntValue(none, "SEXLAB_REPOSITIONMSG_INFO", 1)
		EndIf
	EndIf
	int n = 0
	While(n < Positions.Length)
		ActorAlias[n].GoToState(ActorAlias[n].STATE_PAUSED)
		If (ActorAlias[n] == ActorAlias(PlayerRef))
			ActorAlias[n].TryPauseAndUnlock()
		EndIf
		n += 1
	EndWhile
	Game.SetPlayerAIDriven(false)
	Game.EnablePlayerControls()
	Utility.Wait(1)
	int t = 0
	While(t < 60 && !Input.IsKeyPressed(Config.MoveScene))
		Utility.Wait(0.5)
		t += 1
	EndWhile
	Game.SetPlayerAIDriven()	; make sure player isnt moving before resync
	float x = PlayerRef.X
	float y = PlayerRef.Y
	float z = PlayerRef.Z
	Utility.Wait(0.5)							; wait for momentum to stop
	While(x != PlayerRef.X || y != PlayerRef.Y || z != PlayerRef.Z)
		x = PlayerRef.X
		y = PlayerRef.Y
		z = PlayerRef.Z
		Utility.Wait(0.5)
	EndWhile
	int j = 0
	While(j < Positions.Length)
		ActorAlias[j].TryLockAndUnpause()
		j += 1
	EndWhile
	CenterOnObject(PlayerRef)
	If (!HasPlayer)
		MoveActorsAwayFromPlayer(true)
		Config.DisableThreadControl(self)
	EndIf
EndFunction

Function UpdateAnnotations(string asString)
	String activeScene = GetActiveScene()
	String[] annotations = PapyrusUtil.StringSplit(asString, ",")
	int i = 0
	While(i < annotations.Length)
		SexLabRegistry.AddSceneAnnotation(activeScene, annotations[i])
		i += 1
	EndWhile
EndFunction

int Function GetOffsetIdx(String asOffsetType)
	String[] types = new String[4]
	types[0] = "X"
	types[1] = "Y"
	types[2] = "Z"
	types[3] = "R"
	return types.Find(asOffsetType)
EndFunction

Function SetSceneOffset(float afOffsetValue, String asOffsetType, bool abIncrement = false)
	String activeScene = GetActiveScene()
	int idx = GetOffsetIdx(asOffsetType)
	If (abIncrement)
		afOffsetValue += SexLabRegistry.GetSceneOffset(activeScene)[idx]
	EndIf
	SexLabRegistry.SetSceneOffset(activeScene, afOffsetValue, idx)
	ResetStage()
EndFunction

Function SetStageOffset(Actor akAffectedActor, float afOffsetValue, String asOffsetType, bool abIncrement = false)
	int idx = GetOffsetIdx(asOffsetType)
	int n = GetPositions().Find(akAffectedActor)
	String activeScene = GetActiveScene()
	String activeStage = ""
	If (Config.AdjustStage)
		activeStage = GetActiveStage()
	EndIf
	If (abIncrement)
		afOffsetValue += SexLabRegistry.GetStageOffset(activeScene, activeStage, n)[idx]
	EndIf
	SexLabRegistry.SetStageOffset(activeScene, activeStage, n, afOffsetValue, idx)
	UpdatePlacement(akAffectedActor)
EndFunction

; ------------------------------------------------------- ;
; --- Traditional Hotkeys                             --- ;
; ------------------------------------------------------- ;

int[] Hotkeys
int Property kChangeAnimation   = 0  AutoReadOnly
int Property kMoveScene         = 1  AutoReadOnly
int Property kChangePartner     = 2  AutoReadOnly
int Property kGameRaiseEnj      = 3  AutoReadOnly
int Property kGameHoldback      = 4  AutoReadOnly
int Property kAdvanceAnimation  = 5  AutoReadOnly
int Property kEndAnimation      = 6  AutoReadOnly
int Property kChangePositions   = 7  AutoReadOnly
int Property kOffsetAdjustMode  = 8 AutoReadOnly
int Property kToggleAdjustStage = 9 AutoReadOnly
int Property kRestoreOffsets    = 10 AutoReadOnly
int Property kDirectionUp       = 11 AutoReadOnly
int Property kDirectionDown     = 12 AutoReadOnly
int Property kDirectionLeft     = 13 AutoReadOnly
int Property kDirectionRight    = 14 AutoReadOnly

int Property AdjMode_None     = 0 AutoReadOnly
int Property AdjMode_PosXY    = 1 AutoReadOnly
int Property AdjMode_PosRZ    = 2 AutoReadOnly
int Property AdjMode_SceneXY  = 3 AutoReadOnly
int Property AdjMode_SceneRZ  = 4 AutoReadOnly

Actor _AdjustActor = None     ; The actor (!=PlayerRef) currently selected for adjustments 
int _AdjustMode = 0           ; Determines offsets adjustment mode
bool _SkipHotkeyEvents = False

Function EnableTraditionalHotkeys()
	InitLegacyHotkeys()
	RegisterForKey(Hotkeys[kChangeAnimation])
	RegisterForKey(Hotkeys[kMoveScene])
	RegisterForKey(Hotkeys[kChangePartner])
	If (Config.GameEnabled && HasPlayer)
		RegisterForKey(Hotkeys[kGameRaiseEnj])
		RegisterForKey(Hotkeys[kGameHoldback])
		GetAdjustPos()
	EndIf
	If (!Config.UseSceneMenu)
		RegisterForKey(Hotkeys[kAdvanceAnimation])
		RegisterForKey(Hotkeys[kEndAnimation])
		RegisterForKey(Hotkeys[kChangePositions])
		RegisterForKey(Hotkeys[kOffsetAdjustMode])
		RegisterForKey(Hotkeys[kToggleAdjustStage])
		RegisterForKey(Hotkeys[kRestoreOffsets])
		RegisterForKey(Hotkeys[kDirectionUp])
		RegisterForKey(Hotkeys[kDirectionDown])
		RegisterForKey(Hotkeys[kDirectionLeft])
		RegisterForKey(Hotkeys[kDirectionRight])
	EndIf
EndFunction

Function DisableTraditionalHotkeys()
    int i = 0
    While (i < Hotkeys.Length)
        UnregisterForKey(Hotkeys[i])
        i += 1
    EndWhile
EndFunction

Event OnKeyDown(int aiKey)
	If (Utility.IsInMenuMode() || _SkipHotkeyEvents)
		return
	EndIf
	_SkipHotkeyEvents = true
	bool abModifier = Config.ModifierPressed()
	bool abAdjustTarget = abModifier
	If (aiKey == Hotkeys[kChangeAnimation])
		PickRandomScene("")
	ElseIf (aiKey == Hotkeys[kMoveScene])
		MoveScene()
	ElseIf (aiKey == Hotkeys[kChangePartner])
		ChangeTargetPartner(abModifier)
	EndIf
	If (Config.GameEnabled && HasPlayer)
		If (aiKey == Hotkeys[kGameRaiseEnj])
			ProcessEnjGameArg("Stamina", _AdjustActor, abAdjustTarget)
		ElseIf (aiKey == Hotkeys[kGameHoldback])
			ProcessEnjGameArg("Magicka", _AdjustActor, abAdjustTarget)
		EndIf
	EndIf
	; Legacy
	If (Config.UseSceneMenu)
		_SkipHotkeyEvents = false
		return
	EndIf
	If (aiKey == Hotkeys[kAdvanceAnimation])
		AdvanceStage(abModifier)
	ElseIf (aiKey == Hotkeys[kEndAnimation])
		EndAnimation()
	ElseIf (aiKey == Hotkeys[kChangePositions])
		ChangePositions(false, abAdjustTarget)
	ElseIf (aiKey == Hotkeys[kOffsetAdjustMode])
		CycleOffsetAdjustModes(abModifier)
	ElseIf (aiKey == Hotkeys[kToggleAdjustStage])
		Config.AdjustStage = !Config.AdjustStage
		Debug.Notification("SexLab: AdjustStage: " + Config.AdjustStage)
	ElseIf (aiKey == Hotkeys[kRestoreOffsets])
		RestoreOffsets()
	EndIf
	If (_AdjustMode > AdjMode_None)
		string[] asOffsetType = DetermineOffsetAdjustInputType(aiKey)
		HandleOffsetAdjustment(asOffsetType, aiKey, abAdjustTarget)
	EndIf
	_SkipHotkeyEvents = false
EndEvent

Function InitLegacyHotkeys()
	Hotkeys = new int[15]
	Hotkeys[kChangeAnimation]   = Config.ChangeAnimation
	Hotkeys[kMoveScene]         = Config.MoveScene
	Hotkeys[kChangePartner]     = Config.TargetActor
	Hotkeys[kGameRaiseEnj]      = Config.GameRaiseEnjKey
	Hotkeys[kGameHoldback]      = Config.GameHoldbackKey
	;Legacy
	Hotkeys[kAdvanceAnimation]  = Config.AdvanceAnimation
	Hotkeys[kEndAnimation]      = Config.EndAnimation
	Hotkeys[kChangePositions]   = Config.ChangePositions
	Hotkeys[kOffsetAdjustMode]  = Config.OffsetAdjustMode
	Hotkeys[kToggleAdjustStage] = Config.ToggleAdjustStage
	Hotkeys[kRestoreOffsets]    = Config.RestoreOffsets
	Hotkeys[kDirectionUp]       = Config.DirectionUp
	Hotkeys[kDirectionDown]     = Config.DirectionDown
	Hotkeys[kDirectionLeft]     = Config.DirectionLeft
	Hotkeys[kDirectionRight]    = Config.DirectionRight
EndFunction

; ------------------------------------------------------- ;
; --- Executed Functions [Legacy Hotkeys]             --- ;
; ------------------------------------------------------- ;

Function AdvanceStage(bool abBackwards = false)
	If (!abBackwards)
		GoToStage(Stage + 1)
	ElseIf (Stage > 1)
		GoToStage(Stage - 1)
	EndIf
EndFunction

Function ChangePositions(bool abBackwards = false, bool abAdjustTarget = false)
	int posLen = Positions.Length
	If (posLen < 2)
		return
	EndIf
	String activeScene = GetActiveScene()
	int curIdx = GetAdjustPos()
	Actor curPos = GetIdxPosition(curIdx)
	If (HasPlayer && !abAdjustTarget)
		curIdx = GetPositionIdx(PlayerRef)
		curPos = PlayerRef
	EndIf
	int step = 1
	If (abBackwards)
		step = -1
	EndIf
	int newIdx = (curIdx + step)
	int i = 0
	While (i < posLen - 1)
		If (newIdx >= posLen)
			newIdx = 0
		ElseIf (newIdx < 0)
			newIdx = posLen - 1
		EndIf
		If (SexLabRegistry.CanFillPosition(activeScene, curIdx, GetIdxPosition(newIdx)) && \
			SexLabRegistry.CanFillPosition(activeScene, newIdx, curPos))
			Actor tmpActor = Positions[curIdx]
			Positions[curIdx] = Positions[newIdx]
			Positions[newIdx] = tmpActor
			sslActorAlias tmpAlias = ActorAlias[curIdx]
			ActorAlias[curIdx] = ActorAlias[newIdx]
			ActorAlias[newIdx] = tmpAlias
			SendThreadEvent("PositionChange")
			ResetStage()
			return
		EndIf
		newIdx += step
		i += 1
	EndWhile
	Debug.Notification("Selected actor cannot switch positions")
EndFunction

int Function GetOffsetAdjustMode()
	return _AdjustMode
EndFunction

Function SetOffsetAdjustMode(int aiSet)
	If (aiSet < AdjMode_None || aiSet > AdjMode_SceneRZ)
		return
	EndIf
	_AdjustMode = aiSet
	If (_AdjustMode == AdjMode_None)
		Debug.Notification("SexLab: Disabled offset adjustments")
		return
	EndIf
	;TODO: allign/recenter camera along with world coordinates directions
	If (_AdjustMode == AdjMode_PosXY)
		Debug.Notification("SexLab: Adjusting Position X-Y")
	ElseIf (_AdjustMode == AdjMode_PosRZ)
		Debug.Notification("SexLab: Adjusting Position R-Z")
	ElseIf (_AdjustMode == AdjMode_SceneXY)
		Debug.Notification("SexLab: Adjusting Scene X-Y")
	ElseIf (_AdjustMode == AdjMode_SceneRZ)
		Debug.Notification("SexLab: Adjusting Scene R-Z")
	EndIf
EndFunction

Function CycleOffsetAdjustModes(bool abBackwards = false)
	int modesCount = 5
	int step = 1
	If (abBackwards)
		step = -1
	EndIf
	int aiMode = _AdjustMode + step
	If (aiMode >= modesCount)
		aiMode = 0
	ElseIf (aiMode < 0)
		aiMode = (modesCount - 1)
	EndIf
	SetOffsetAdjustMode(aiMode)
EndFunction

string[] Function DetermineOffsetAdjustInputType(int aiKey)
	string[] ret = Utility.CreateStringArray(2, "")
	If ((aiKey != Config.DirectionUp) && (aiKey != Config.DirectionDown) && \
		(aiKey != Config.DirectionLeft) && (aiKey != Config.DirectionRight))
		return ret
	EndIf
	bool abAdjustingRZ = (_AdjustMode == AdjMode_PosRZ) || (_AdjustMode == AdjMode_SceneRZ)
	If (aiKey == Config.DirectionLeft)
		ret[0] = "-"
		ret[1] = "R"
		If (!abAdjustingRZ)
			ret[1] = "X"
		EndIf
	ElseIf (aiKey == Config.DirectionRight)
		ret[1] = "R"
		If (!abAdjustingRZ)
			ret[1] = "X"
		EndIf
	ElseIf (aiKey == Config.DirectionUp)
		ret[1] = "Z"
		If (!abAdjustingRZ)
			ret[1] = "Y"
		EndIf
	ElseIf (aiKey == Config.DirectionDown)
		ret[0] = "-"
		ret[1] = "Z"
		If (!abAdjustingRZ)
			ret[1] = "Y"
		EndIf
	EndIf
	return ret
EndFunction

Function HandleOffsetAdjustment(String[] asOffsetType, int aiKey, bool abAdjustTarget)
	If (asOffsetType[1] == "")
		return
	EndIf
	PauseTimer(true)
	Actor akAffectedActor = _AdjustActor
	If (HasPlayer && !abAdjustTarget)
		akAffectedActor = PlayerRef
	EndIf
	float afValue = Config.AdjustStepSize
	bool abAdjustingPos = (_AdjustMode == AdjMode_PosXY)  || (_AdjustMode == AdjMode_PosRZ)
	ApplyOffsetAdjustment(akAffectedActor, afValue, asOffsetType, abAdjustingPos)
	If (!Input.IsKeyPressed(aiKey))
		PauseTimer(false)
		return
	EndIf
	While (Input.IsKeyPressed(aiKey))
		Utility.Wait(0.4)
		If (!Input.IsKeyPressed(aiKey))
			PauseTimer(false)
			return
		EndIf
		ApplyOffsetAdjustment(akAffectedActor, afValue, asOffsetType, abAdjustingPos)
	EndWhile
EndFunction

Function ApplyOffsetAdjustment(Actor akAffectedActor, float afValue, String[] asOffsetType, bool abAdjustingPos)
	If ((asOffsetType[1] == "") || (afValue == 0))
		return
	EndIf
	If (asOffsetType[0] == "-")
		afValue = -afValue
	EndIf
	If (abAdjustingPos)
		SetStageOffset(akAffectedActor, afValue, asOffsetType[1], true)
	Else
		SetSceneOffset(afValue, asOffsetType[1], true)
	EndIf
EndFunction

Function RestoreOffsets()
	SexLabRegistry.ResetSceneOffset(GetActiveScene())
	SexLabRegistry.ResetStageOffsetA(GetActiveScene(), GetActiveStage())
	RealignActors()
EndFunction

int Function GetAdjustPos()
	If (_AdjustActor)
		return GetPositionIdx(_AdjustActor)
	EndIf
	int AdjustIdx = -1
	If (Config.TargetRef)
		AdjustIdx = GetPositionIdx(Config.TargetRef)
	Else
		If (HasPlayer)
			AdjustIdx = IndexTravelComplex(GetPositionIdx(PlayerRef))
		Else
			AdjustIdx = (GetPositions().Length > 1) as int
		EndIf
	EndIf
	_AdjustActor = GetIdxPosition(AdjustIdx)
	Config.SetTargetActor(_AdjustActor)
	return AdjustIdx
EndFunction

Actor Function GetTargetPartner()
	If (!_AdjustActor)
		GetAdjustPos()
	EndIf
	return _AdjustActor
EndFunction

Function ChangeTargetPartner(bool abBackwards = false)
	If (Positions.Length < 3)
		return
	EndIf
	int curIdx = GetAdjustPos()
	int newIdx = IndexTravelComplex(curIdx, abBackwards, PlayerRef)
	_AdjustActor = GetIdxPosition(newIdx)
	Config.SetTargetActor(_AdjustActor)
	Config.SelectedSpell.Cast(_AdjustActor)	; SFX for visual feedback
	PlayHotkeyFX(0, !abBackwards)
	Log("ChangeTargetPartner(), currently focused partner: " + SexLabUtil.ActorName(_AdjustActor))
EndFunction

Function PlayHotkeyFX(int i, bool abBackwards)
	If (abBackwards)
		Config.HotkeyDown[i].Play(PlayerRef)
	Else
		Config.HotkeyUp[i].Play(PlayerRef)
	EndIf
EndFunction

; *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-* ;
; ----------------------------------------------------------------------------- ;
;				██╗     ███████╗ ██████╗  █████╗  ██████╗██╗   ██╗				;
;				██║     ██╔════╝██╔════╝ ██╔══██╗██╔════╝╚██╗ ██╔╝				;
;				██║     █████╗  ██║  ███╗███████║██║      ╚████╔╝ 				;
;				██║     ██╔══╝  ██║   ██║██╔══██║██║       ╚██╔╝  				;
;				███████╗███████╗╚██████╔╝██║  ██║╚██████╗   ██║   				;
;				╚══════╝╚══════╝ ╚═════╝ ╚═╝  ╚═╝ ╚═════╝   ╚═╝   				;
; ----------------------------------------------------------------------------- ;
; *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-* ;

;int[] Hotkeys
;int Property kAdvanceAnimation = 0 AutoReadOnly
;int Property kChangeAnimation  = 1 AutoReadOnly
;int Property kChangePositions  = 2 AutoReadOnly
;int Property kAdjustChange     = 3 AutoReadOnly
int Property kAdjustForward    = 4 AutoReadOnly
int Property kAdjustSideways   = 5 AutoReadOnly
int Property kAdjustUpward     = 6 AutoReadOnly
;int Property kRealignActors    = 7 AutoReadOnly
;int Property kRestoreOffsets   = 8 AutoReadOnly
;int Property kMoveScene        = 9 AutoReadOnly
int Property kRotateScene      = 10 AutoReadOnly
;int Property kEndAnimation     = 11 AutoReadOnly
;int Property kAdjustSchlong    = 12 AutoReadOnly
;/
Event OnKeyDown(int KeyCode)
	If(Utility.IsInMenuMode() || _SkipHotkeyEvents)
		return
	EndIf
	_SkipHotkeyEvents = true
	int hotkey = Hotkeys.Find(KeyCode)
	If(hotkey == kAdvanceAnimation)
		If (Config.BackwardsPressed())
			AdvanceStage(true)
		Else
			AdvanceStage(false)
		EndIf
	ElseIf(hotkey == kChangeAnimation)
		ChangeAnimation(Config.BackwardsPressed())
	ElseIf(hotkey == kAdjustForward)
		AdjustForward(Config.BackwardsPressed(), Config.AdjustStagePressed())
	ElseIf(hotkey == kAdjustUpward)
		AdjustUpward(Config.BackwardsPressed(), Config.AdjustStagePressed())
	ElseIf(hotkey == kAdjustSideways)
		AdjustSideways(Config.BackwardsPressed(), Config.AdjustStagePressed())
	ElseIf(hotkey == kRotateScene)
		RotateScene(Config.BackwardsPressed())
	ElseIf(hotkey == kAdjustSchlong)
		; AdjustSchlongEx(Config.BackwardsPressed(), Config.AdjustStagePressed())
	ElseIf(hotkey == kAdjustChange) ; Change Adjusting Position
		AdjustChange(Config.BackwardsPressed())
	ElseIf(hotkey == kRealignActors)
		RealignActors()
	ElseIf(hotkey == kChangePositions)
		ChangePositions()
	ElseIf(hotkey == kRestoreOffsets)
		RestoreOffsets()
	ElseIf(hotkey == kMoveScene)
		MoveScene()
	ElseIf(hotkey == kEndAnimation)
		EndAnimation()
	EndIf
	_SkipHotkeyEvents = false
EndEvent
/;

Function ChangeAnimation(bool backwards = false)
	return PickRandomScene("")
EndFunction

Function AdjustCoordinate(bool abBackwards, bool abStageOnly, float afValue, int aiKeyIdx, int aiOffsetType)
	; aiOffsetType := [X, Y, Z, Rotation]
	UnregisterForUpdate()
	String scene_ = GetActiveScene()
	String stage_ = ""
	If (!abStageOnly)
		stage_ = GetActiveStage()
	EndIf
	int AdjustPos = GetAdjustPos()
	bool first_pass = true
	While(true)
		PlayHotkeyFX(0, abBackwards)
		SexLabRegistry.SetStageOffset(scene_, stage_, AdjustPos, afValue, aiOffsetType)
		; UpdatePlacement(AdjustAlias.GetActorReference())
		Utility.Wait(0.1)
		If(!Input.IsKeyPressed(Hotkeys[aiKeyIdx]))
			UpdateTimer(5)
			OnUpdate()
			return
		ElseIf (first_pass)
			first_pass = false
			Utility.Wait(0.4)
		EndIf
	EndWhile
EndFunction
Function AdjustForward(bool backwards = false, bool AdjustStage = false)
	float value = 0.5 - (backwards as float)
	AdjustCoordinate(backwards, AdjustStage, value, kAdjustForward, 0)
EndFunction
Function AdjustSideways(bool backwards = false, bool AdjustStage = false)
	float value = 0.5 - (backwards as float)
	AdjustCoordinate(backwards, AdjustStage, value, kAdjustSideways, 1)
EndFunction
Function AdjustUpward(bool backwards = false, bool AdjustStage = false)
	float value = 0.5 - (backwards as float)
	AdjustCoordinate(backwards, AdjustStage, value, kAdjustUpward, 2)
EndFunction

Function RotateScene(bool backwards = false)
	float Amount = 15.0
	If(Config.IsAdjustStagePressed())
		Amount = 180.0
	ElseIf(backwards)
		Amount = -15.0
	EndIf
	
	bool first_pass = true
	While(true)
		PlayHotkeyFX(1, !backwards)
		float[] coords
		coords[5] = coords[5] + Amount
		If(coords[5] >= 360.0)
			coords[5] = coords[5] - 360.0
		ElseIf(coords[5] < 0.0)
			coords[5] = coords[5] + 360.0
		EndIf
		CenterOnCoords(coords[0], coords[1], coords[2], 0, 0, coords[5], true)
		Utility.Wait(0.03)
		If(!Input.IsKeyPressed(Hotkeys[kRotateScene]))
			RegisterForSingleUpdate(0.2)
			return
		ElseIf (first_pass)
			first_pass = false
			Utility.Wait(0.4)
		EndIf
	EndWhile
EndFunction

Function AdjustChange(bool backwards = false)
	ChangeTargetPartner(backwards)
EndFunction

float Function GetAnimationRunTime()
	return Animation.GetTimersRunTime(Timers)
EndFunction

Function ResetPositions()
	RealignActors()
EndFunction

ObjectReference Function GetCenterFX()
	if CenterRef != none && CenterRef.Is3DLoaded()
		return CenterRef
	else
		int i = 0
		while i < ActorCount
			if Positions[i] != none && Positions[i].Is3DLoaded()
				return Positions[i]
			endIf
			i += 1
		endWhile
	endIf
EndFunction

Function AdjustSchlong(bool backwards = false)
	; AdjustSchlongEx(backwards, true)
EndFunction

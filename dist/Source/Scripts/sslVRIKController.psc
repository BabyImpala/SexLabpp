ScriptName sslVRIKController extends sslThreadController
{
	Utility script for VRIK thread controls
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

sslVRIKConfig VRConfig 
bool _SkipGestureEvents = False
bool _AdjustSelf = True

Function RegisterGesture(int aiGesture, String asName)
	VRIK.VrikSetProfileAction(aiGesture, "SLVR_"+asName)
	RegisterForModEvent("SLVR_"+asName, "VRHandleGesture")
EndFunction

Function UnregisterGesture(String asName)
	UnregisterForModEvent("SLVR_" + asName)
EndFunction

Function EnableGesturesVR()
	VRConfig = Config as sslVRIKConfig
	If (!VRConfig.UseGestures)
		return
	EndIf
	VRIK.VrikBeginGestureProfile()
	; L1 -> EnjGame + TargetActor
	RegisterGesture(1, "ToggleAdjustSelf")          ; L (tap) = Left Thumbstick Press or Trackpad Tap
	If (Config.GameEnabled && HasPlayer)
		RegisterGesture(2, "GameRaiseEnj")          ; L + up
		RegisterGesture(3, "GameHoldback")          ; L + down
	EndIf
	RegisterGesture(4, "TargetPartnerPrev")         ; L + left
	RegisterGesture(5, "TargetPartnerNext")         ; L + right
	RegisterGesture(6, "POVFirstPerson")            ; L + back
	RegisterGesture(7, "POVThirdPerson")            ; L + forward
	; R1 -> SceneControl Main
	RegisterGesture(14, "ToggleCollision")          ; R (tap) = Right Thumbstick Press or Trackpad Tap
	RegisterGesture(15, "SceneChange")              ; R + up
	RegisterGesture(16, "SceneEnd")                 ; R + down
	RegisterGesture(17, "StagePrev")                ; R + left
	RegisterGesture(18, "StageNext")                ; R + right
	; L2 -> OffsetAdjust Inputs
	RegisterGesture(27, "AdjustStageToggle")        ; L2 (tap) = Left Index Touchpad Press
	RegisterGesture(28, "OffsetUp")                 ; L2 + up
	RegisterGesture(29, "OffsetDown")               ; L2 + down
	RegisterGesture(30, "OffsetLeft")               ; L2 + left
	RegisterGesture(31, "OffsetRight")              ; L2 + right
	; R2 -> SceneControl Complex
	RegisterGesture(40, "RestoreOffsets")           ; R2 (tap) = Right Index Touchpad Press
	RegisterGesture(41, "AdjOffsetModeNext")        ; R2 + up
	RegisterGesture(42, "AdjOffsetModePrev")        ; R2 + down
	RegisterGesture(43, "ChangePosForward")         ; R2 + left
	RegisterGesture(44, "ChangePosBackward")        ; R2 + right
	RegisterGesture(45, "MoveScene")                ; R2 + back
EndFunction

Function DisableGesturesVR()
	If (!VRConfig.UseGestures)
		return
	EndIf
	VRIK.VrikEndGestureProfile()
	UnregisterGesture("ToggleAdjustSelf")
	UnregisterGesture("GameRaiseEnj")
	UnregisterGesture("GameHoldback")
	UnregisterGesture("TargetPartnerPrev")
	UnregisterGesture("TargetPartnerNext")
	UnregisterGesture("POVFirstPerson")
	UnregisterGesture("POVThirdPerson")
	UnregisterGesture("ToggleCollision")
	UnregisterGesture("SceneChange")
	UnregisterGesture("SceneEnd")
	UnregisterGesture("StagePrev")
	UnregisterGesture("StageNext")
	UnregisterGesture("AdjustStageToggle")
	UnregisterGesture("OffsetUp")
	UnregisterGesture("OffsetDown")
	UnregisterGesture("OffsetLeft")
	UnregisterGesture("OffsetRight")
	UnregisterGesture("RestoreOffsets")
	UnregisterGesture("AdjOffsetModeNext")
	UnregisterGesture("AdjOffsetModePrev")
	UnregisterGesture("ChangePosForward")
	UnregisterGesture("ChangePosBackward")
	UnregisterGesture("MoveScene")
EndFunction

Function VRHandleGesture(String asEventName, String Foobar, float Presses, Form Sender)
	If (Utility.IsInMenuMode() || _SkipGestureEvents)
		return
	EndIf
	_SkipGestureEvents = true
	bool abAdjustTarget = !_AdjustSelf
	If (HasPlayer && Config.GameEnabled)
		If (asEventName == "SLVR_GameRaiseEnj")
			ProcessEnjGameArg("Stamina", GetTargetPartner(), abAdjustTarget)
		ElseIf (asEventName == "SLVR_GameHoldback")
			ProcessEnjGameArg("Magicka", GetTargetPartner(), abAdjustTarget)
		EndIf
	EndIf
	If (asEventName == "ToggleAdjustSelf")
		_AdjustSelf = !_AdjustSelf
		Debug.Notification("SexLab: AdjustSelf: " + _AdjustSelf)
	ElseIf (asEventName == "SLVR_TargetPartnerPrev")
		ChangeTargetPartner(true)
	ElseIf (asEventName == "SLVR_TargetPartnerNext")
		ChangeTargetPartner()
	ElseIf (asEventName == "SLVR_POVFirstPerson")
		VRConfig.ToggleVRIK(true, 0)
	ElseIf (asEventName == "SLVR_POVThirdPerson")
		VRConfig.ToggleVRIK(true, 1)
	ElseIf (asEventName == "SLVR_ToggleCollision")
		If (HasPlayer && VRConfig.Use3rdPerson)
			VRConfig.NoCollision = !VRConfig.NoCollision
			Utility.SetIniBool("bDisablePlayerCollision:Havok", VRConfig.NoCollision)
		EndIf
	ElseIf (asEventName == "SLVR_SceneChange")
		PickRandomScene("")
	ElseIf (asEventName == "SLVR_SceneEnd")
		EndAnimation()
	ElseIf (asEventName == "SLVR_StagePrev")
		AdvanceStage(true)
	ElseIf (asEventName == "SLVR_StageNext")
		AdvanceStage()
	ElseIf (asEventName == "SLVR_AdjustStageToggle")
		Config.AdjustStage = !Config.AdjustStage
		Debug.Notification("SexLab: AdjustStage: " + Config.AdjustStage)
	ElseIf (asEventName == "SLVR_RestoreOffsets")
		RestoreOffsets()
	ElseIf (asEventName == "SLVR_AdjOffsetModeNext")
		CycleOffsetAdjustModes()
	ElseIf (asEventName == "SLVR_AdjOffsetModePrev")
		CycleOffsetAdjustModes(true)
	ElseIf (asEventName == "SLVR_ChangePosForward")
		ChangePositions(false, abAdjustTarget)
	ElseIf (asEventName == "SLVR_ChangePosBackward")
		ChangePositions(true, abAdjustTarget)
	ElseIf (asEventName == "SLVR_MoveScene")
		MoveScene()
	EndIf
	If (GetOffsetAdjustMode() > AdjMode_None)
		string[] asOffsetType = Utility.CreateStringArray(2, "")
		If (asEventName == "SLVR_OffsetUp")
			asOffsetType = DetermineOffsetAdjustInputType(Config.DirectionUp)
		ElseIf (asEventName == "SLVR_OffsetDown")
			asOffsetType = DetermineOffsetAdjustInputType(Config.DirectionDown)
		ElseIf (asEventName == "SLVR_OffsetLeft")
			asOffsetType = DetermineOffsetAdjustInputType(Config.DirectionLeft)
		ElseIf (asEventName == "SLVR_OffsetRight")
			asOffsetType = DetermineOffsetAdjustInputType(Config.DirectionRight)
		EndIf
		HandleOffsetAdjustmentVR(asOffsetType, abAdjustTarget)
	EndIf
	_SkipGestureEvents = false
EndFunction

Function HandleOffsetAdjustmentVR(String[] asOffsetType, bool abAdjustTarget)
	If (asOffsetType[1] == "")
		return
	EndIf
	PauseTimer(true)
	Actor akAffectedActor = GetTargetPartner()
	If (HasPlayer && !abAdjustTarget)
		akAffectedActor = PlayerRef
	EndIf
	float afValue = Config.AdjustStepSize
	int aiAdjMode = GetOffsetAdjustMode()
	bool abAdjustingPos = (aiAdjMode == AdjMode_PosXY)  || (aiAdjMode == AdjMode_PosRZ)
	ApplyOffsetAdjustment(akAffectedActor, afValue, asOffsetType, abAdjustingPos)
	float refX = VRIK.VrikGetHandX(true)
	float refY = VRIK.VrikGetHandY(true)
	float refZ = VRIK.VrikGetHandZ(true)
	float newX = 0.0
	float newY = 0.0
	float newZ = 0.0
	float curDrift = 0.0
	While (VRIK.VrikIsTriggerPressed(true))
		newX = VRIK.VrikGetHandX(true)
		newY = VRIK.VrikGetHandY(true)
		newZ = VRIK.VrikGetHandZ(true)
		curDrift = Math.Abs(newX - refX) + Math.Abs(newY - refY) + Math.Abs(newZ - refZ)
		If (curDrift > 30.0) ;hand moved too much, not roughly in place
			PauseTimer(false)
			return
		EndIf
		ApplyOffsetAdjustment(akAffectedActor, afValue, asOffsetType, abAdjustingPos)
		Utility.Wait(0.02)
		afValue += Config.AdjustStepSize * 0.1
		If (afValue > Config.AdjustStepSize * 5.0)
			afValue = Config.AdjustStepSize * 5.0
		EndIf
	EndWhile
	If (VRConfig.GestureHaptics)
		VRIK.VrikHapticPulse(true, 2, 800)
	EndIf
	PauseTimer(false)
EndFunction

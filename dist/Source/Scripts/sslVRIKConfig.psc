ScriptName sslVRIKConfig extends sslSystemConfig
{
	Utility script for VRIK configs
}

bool Function CheckForSkyrimVR() global
	int iSKSE = SKSE.GetVersion()*10000 + SKSE.GetVersionMinor()*100 + SKSE.GetVersionBeta()
	return (iSKSE == 20012) ;SKSE VR v2.0.12
EndFunction

bool Function CheckForVRIK() global
	int iVRIK = VRIK.VrikGetBuildNumber()
	return (iVRIK >= 80123)
EndFunction

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

bool _b3rdPerson
bool _bLockHeight
float _fHeightAdjSpeed
bool _bTrackHead
int _iTrackHands
float _fDistHideHead
float _fDistNearClip
bool _aLockHmdToBody
float _fLockHmdDistance
float _fLockHmdTolerance
float _fLockHmdSpeed

Function RefreshConfigsVRIK(bool abOverrideConfig=false, int ab3rdPerson=-1, \
	int abLockHeight=-1, float afHeightAdjSpeed=-1.0, int abTrackHead=-1, int aiTrackHands=-1, \
	float afDistHideHead=-1.0, float afDistNearClip=-1.0, int abLockHmdToBody=-1, \
	float afLockHmdDistance=-1.0, float afLockHmdTolerance=-1.0, float afLockHmdSpeed=-1.0)
  ;Handle primary determiners first
  _b3rdPerson = Use3rdPerson
  _aLockHmdToBody = LockHmdToBody
  If (abOverrideConfig)
    If (ab3rdPerson != -1)
      _b3rdPerson = ab3rdPerson as bool
    EndIf
    If (abLockHmdToBody != -1)
      _aLockHmdToBody = abLockHmdToBody as bool
    EndIf
  EndIf
  ;Load defaults
  _bLockHeight = true
  _fHeightAdjSpeed = HeightAdjustSpeed
  _bTrackHead = TrackHead
  _iTrackHands = TrackHands
  _fDistHideHead = DistanceHideHead
  _fDistNearClip = DistanceNearClip
  _fLockHmdDistance = LockHmdDistance
  _fLockHmdTolerance = LockHmdTolerance
  _fLockHmdSpeed = LockHmdSpeed
  NoCollision = true
  ;Load first person non-HmdToBody configs
  If (!_b3rdPerson && !_aLockHmdToBody)
    _bTrackHead = false
    _iTrackHands = 0
    _fLockHmdDistance = 0.0
    _fLockHmdTolerance = 0.0
    _fLockHmdSpeed = 150.0
  EndIf
  ;Load third person configs
  If (_b3rdPerson)
    _bLockHeight = LockHeight
    _bTrackHead = false
    _iTrackHands = 0
    _fDistHideHead = 2.0
    _aLockHmdToBody = false
    _fLockHmdDistance = 500.0
    _fLockHmdTolerance = 500.0
    _fLockHmdSpeed = 60.0
    NoCollision = false
    If (AutoTFC)
      NoCollision = true
    EndIf
  EndIf
  ;Handle overrides, if any
  If (abOverrideConfig)
    If (abLockHeight != -1)
      _bLockHeight = abLockHeight as bool
    EndIf
    If (afHeightAdjSpeed != -1.0)
      _fHeightAdjSpeed = afHeightAdjSpeed
    EndIf
    If (abTrackHead != -1)
      _bTrackHead = abTrackHead as bool
    EndIf
    If (aiTrackHands != -1)
      _iTrackHands = aiTrackHands
    EndIf
    If (afDistHideHead != -1.0)
      _fDistHideHead = afDistHideHead
    EndIf
    If (afDistNearClip != -1.0)
      _fDistNearClip = afDistNearClip
    EndIf
    If (afLockHmdDistance != -1.0)
      _fLockHmdDistance = afLockHmdDistance
    EndIf
    If (afLockHmdTolerance != -1.0)
      _fLockHmdTolerance = afLockHmdTolerance
    EndIf
    If (afLockHmdSpeed != -1.0)
      _fLockHmdSpeed = afLockHmdSpeed
    EndIf
  EndIf
EndFunction

Function ApplyConfigsVRIK(bool abEnabled)
  If (!abEnabled)
    NoCollision = false
    Utility.SetIniBool("bComfortSneak:VR", false)
    Utility.SetIniBool("bDisablePlayerCollision:Havok", false)
    VRIK.VrikRestoreSettings()
    return
  EndIf
  float afScaleBody = Game.GetPlayer().GetScale()
  ;float afScaleVR = VRIK.VrikGetSetting("bodySize")
  If (ScaleVRBody)
    VRIK.VrikSetSetting("bodySize", afScaleBody)
    VRIK.VrikSetSetting("armSize", afScaleBody)
    VRIK.VrikSetSetting("armLength", afScaleBody) ; or 1.0?
  EndIf
  ; Constant
  Utility.SetIniBool("bComfortSneak:VR", true)
  VRIK.VrikSetSetting("enablePosture", 0)
  VRIK.VrikSetSetting("enableBody", 0)
  VRIK.VrikSetSetting("enableJumping", 0)
  VRIK.VrikSetSetting("displayHolsters", 0)
  VRIK.VrikSetSetting("lockRotation", 1)
  ; Mode Dependent
  VRIK.VrikSetSetting("lockHeightToBody", _bLockHeight as int)
  VRIK.VrikSetSetting("heightAdjustSpeed", _fHeightAdjSpeed)
  VRIK.VrikSetSetting("enableHead", _bTrackHead as int)
  If (_iTrackHands > 0)
    VRIK.VrikSetSetting("enableLeftArm", 1)
    VRIK.VrikSetSetting("enableRightArm", 1)
    VRIK.VrikSetSetting("enableInteractiveHands", _iTrackHands - 1)
  Else
    VRIK.VrikSetSetting("enableLeftArm", 0)
    VRIK.VrikSetSetting("enableRightArm", 0)
  EndIf
  VRIK.VrikSetSetting("hidePlayerHeadDistance", _fDistHideHead)
  VRIK.VrikSetSetting("nearClipDistance", _fDistNearClip)
  Utility.SetIniFloat("fNearDistance:Display", _fDistNearClip)
  If (_aLockHmdToBody)
    VRIK.VrikSetSetting("lockHmdToBody", 1)
    VRIK.VrikSetSetting("lockHmdMinThreshold", _fLockHmdDistance)
    VRIK.VrikSetSetting("lockHmdMaxThreshold", _fLockHmdTolerance)
    VRIK.VrikSetSetting("lockHmdSpeed", _fLockHmdSpeed)
  EndIf
  Utility.SetIniBool("bDisablePlayerCollision:Havok", NoCollision)
  VRIK.VrikSetGesture("enableGestureHaptics", GestureHaptics as int)
EndFunction

int Function ToggleVRIK(bool abEnabled, int ai3rdPerson = -1)
  int VRIKRestoreInTicks = 0
  If (!abEnabled)
    ApplyConfigsVRIK(false)
    Game.EnablePlayerControls(true, true, true, false, true, false, true, false, 0)
		Game.SetPlayerAIDriven(false)
    return VRIKRestoreInTicks
  EndIf
  int tempLockHmd = -1
  If ((ai3rdPerson > -1) && (Use3rdPerson != ai3rdPerson as bool))
    Use3rdPerson = ai3rdPerson as bool
  EndIf
  If (Use3rdPerson)
    Game.SetPlayerAIDriven(false)
  Else
    Game.SetPlayerAIDriven(true)
    If (!LockHmdToBody)
      tempLockHmd = 1
      VRIKRestoreInTicks = 3 ; t=1.5s
    EndIf
  EndIf
  Game.EnablePlayerControls(true, false, false, true, false, true, false, true, 0)
  Game.DisablePlayerControls(false, true, true, false, true, false, true, false, 0)
  RefreshConfigsVRIK(true, abLockHmdToBody=tempLockHmd)
  ApplyConfigsVRIK(true)
  return VRIKRestoreInTicks
EndFunction

int Function UpdatePositioningVRIK(int VRIKRestoreInTicks)
  Actor PlayerRef = Game.GetPlayer()
  VRIK.VrikSetSetting("lockRotationAngle", PlayerRef.GetAngleZ())
  VRIK.VrikSetSetting("lockPositionX", PlayerRef.X)
  VRIK.VrikSetSetting("lockPositionY", PlayerRef.Y)
  VRIK.VrikSetSetting("lockPositionZ", PlayerRef.Z)
  VRIK.VrikSetSetting("lockPosition", 2)
  If (!Use3rdPerson)
    VRIK.VrikSetSetting("rotateHmdToBodySeconds", 1.5)
    If (!LockHmdToBody)
      VRIK.VrikSetSetting("lockHmdToBody", 1) ;temp override
      If (VRIKRestoreInTicks < 3)
        VRIKRestoreInTicks = 3 ; t=1.5s
      EndIf
    EndIf
  EndIf
  return VRIKRestoreInTicks
EndFunction

Function RestoreHmdVRIK()
  VRIK.VrikSetSetting("lockHmdToBody", 2)
  VRIK.VrikSetSetting("lockPosition", 0)
EndFunction

Function DoWhiteOutEfffect(Actor akActor, int aiOrgasms)
  If (!IsSkyrimVR || !OrgasmWhiteout || (akActor != Game.GetPlayer()))
    return
  EndIf
  bool abKO = (akActor.GetActorValuePercentage("Stamina") < 0.25)
  float HoldTime = (aiOrgasms as float) + 1.0
  If (HoldTime > 4.0)
    HoldTime = 4.0
  EndIf
  Game.FadeOutGame(true, abKO, 0.0, 2.0)
  Utility.WaitMenuMode(0.5)
  Game.FadeOutGame(false, false, HoldTime, 2.0)
EndFunction

; ------------------------------------------------------- ;
; --- Helper Functions                                --- ;
; ------------------------------------------------------- ;

int Function GetConfigInt(String asSetting)
    return sslSystemConfig.GetSettingInt(asSetting)
EndFunction
float Function GetConfigFlt(String asSetting)
    return sslSystemConfig.GetSettingFlt(asSetting)
EndFunction
bool Function GetConfigBool(String asSetting)
    return sslSystemConfig.GetSettingBool(asSetting)
EndFunction
Function SetConfigInt(String asSetting, int aiValue)
    sslSystemConfig.SetSettingInt(asSetting, aiValue)
EndFunction
Function SetConfigFlt(String asSetting, float afValue)
    sslSystemConfig.SetSettingFlt(asSetting, afValue)
EndFunction
Function SetConfigBool(String asSetting, bool abValue)
    sslSystemConfig.SetSettingBool(asSetting, abValue)
EndFunction

; ------------------------------------------------------- ;
; --- General Configs                                 --- ;
; ------------------------------------------------------- ;

bool Property UseGestures hidden
  bool Function Get()
    return GetConfigBool("bVRGestures")
  EndFunction
  Function Set(bool value)
    SetConfigBool("bVRGestures", value)
  EndFunction
EndProperty
bool Property GestureHaptics hidden
  bool Function Get()
    return GetConfigBool("bVRGestureHaptics")
  EndFunction
  Function Set(bool value)
    SetConfigBool("bVRGestureHaptics", value)
  EndFunction
EndProperty
bool Property ScaleVRBody hidden
  bool Function Get()
    return GetConfigBool("bVRScaleBody")
  EndFunction
  Function Set(bool value)
    SetConfigBool("bVRScaleBody", value)
  EndFunction
EndProperty
bool Property OrgasmWhiteout hidden
  bool Function Get()
    return GetConfigBool("bVROrgasmFX")
  EndFunction
  Function Set(bool value)
    SetConfigBool("bVROrgasmFX", value)
  EndFunction
EndProperty
bool Property NoCollision hidden
  bool Function Get()
    return GetConfigBool("bVRNoCollision")
  EndFunction
  Function Set(bool value)
    SetConfigBool("bVRNoCollision", value)
  EndFunction
EndProperty

; ------------------------------------------------------- ;
; --- VRIK Configs                                    --- ;
; ------------------------------------------------------- ;

bool Property Use3rdPerson hidden
  bool Function Get()
    return GetConfigBool("b3rdPersonVR")
  EndFunction
  Function Set(bool value)
    SetConfigBool("b3rdPersonVR", value)
  EndFunction
EndProperty
bool Property LockHeight hidden
  bool Function Get()
    return GetConfigBool("bLockHeightVR")
  EndFunction
  Function Set(bool value)
    SetConfigBool("bLockHeightVR", value)
  EndFunction
EndProperty
bool Property TrackHead hidden
  bool Function Get()
    return GetConfigBool("bTrackHeadVR")
  EndFunction
  Function Set(bool value)
    SetConfigBool("bTrackHeadVR", value)
  EndFunction
EndProperty
int Property TrackHands hidden
  int Function Get()
    return GetConfigInt("iTrackHandsVR")
  EndFunction
  Function Set(int aiSet)
    SetConfigInt("iTrackHandsVR", aiSet)
  EndFunction
EndProperty
float Property HeightAdjustSpeed hidden
  float Function Get()
    return GetConfigFlt("fHeightAdjSpeedVR")
  EndFunction
  Function Set(float afSet)
    SetConfigFlt("fHeightAdjSpeedVR", afSet)
  EndFunction
EndProperty 
float Property DistanceHideHead hidden
  float Function Get()
    return GetConfigFlt("fDistHideHeadVR")
  EndFunction
  Function Set(float afSet)
    SetConfigFlt("fDistHideHeadVR", afSet)
  EndFunction
EndProperty
float Property DistanceNearClip hidden
  float Function Get()
    return GetConfigFlt("fDistNearClipVR")
  EndFunction
  Function Set(float afSet)
    SetConfigFlt("fDistNearClipVR", afSet)
  EndFunction
EndProperty

;lockHmdToBody (lock view to body)
bool Property LockHmdToBody hidden
  bool Function Get()
    return GetConfigBool("bLockHmdToBody")
  EndFunction
  Function Set(bool value)
    SetConfigBool("bLockHmdToBody", value)
  EndFunction
EndProperty
float Property LockHmdDistance hidden
  float Function Get()
    return GetConfigFlt("fLockHmdDistance")
  EndFunction
  Function Set(float afSet)
    SetConfigFlt("fLockHmdDistance", afSet)
  EndFunction
EndProperty
float Property LockHmdTolerance hidden
  float Function Get()
    return GetConfigFlt("fLockHmdTolerance")
  EndFunction
  Function Set(float afSet)
    SetConfigFlt("fLockHmdTolerance", afSet)
  EndFunction
EndProperty
float Property LockHmdSpeed hidden
  float Function Get()
    return GetConfigFlt("fLockHmdSpeed")
  EndFunction
  Function Set(float afSet)
    SetConfigFlt("fLockHmdSpeed", afSet)
  EndFunction
EndProperty

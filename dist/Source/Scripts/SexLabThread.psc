ScriptName SexLabThread extends sslBaseObject Hidden
{
    Base script for all thread related classes. 
    Provides interaction detection API using standardized InterType system.
    
    The InterType system (0-27) replaces the legacy CType system entirely.
    All collision detection, interaction guessing, and velocity calculations
    are now handled in C++ through the dispatch layer.
}

; =============================================================== ;
; === INTERACTION TYPE CONSTANTS (InterType System 0-27)        === ;
; =============================================================== ;
;/
    Standardized interaction types - obfuscated names for internal use.
    These replace the legacy CType system entirely.
    Range: 0-27 (SUPPORTED_INTER_COUNT = 28)
    
    Naming convention:
    - pXXX = position actor receiving/doing XXX
    - aXXX = position actor's appendage doing XXX  
    - bXXX = bidirectional interaction (both actors)
/;

;--> Stimulation types (actor receiving non-penile stimulation)
int Property t00 = 0 AutoReadOnly Hidden  ; pStimulation: pos_crotch is being fingered, fisted, or toys_inserted
int Property t01 = 1 AutoReadOnly Hidden  ; aAnimObjFace: pos_anim_obj is in front of partner's face
int Property t02 = 2 AutoReadOnly Hidden  ; pAnimObjFace: pos_face is in front of partner's anim_obj

;--> Body receiving/doing something
int Property t03 = 3 AutoReadOnly Hidden  ; pSuckingToes: pos_toes are closer to partner's mouth
int Property t04 = 4 AutoReadOnly Hidden  ; pGrinding: pos_body is being grinded against by partner's crotch
int Property t05 = 5 AutoReadOnly Hidden  ; pSkullfuck: pos_head is being penetrated by partner's pp
int Property t06 = 6 AutoReadOnly Hidden  ; aHandJob: pos_hand is moving around partner's pp
int Property t07 = 7 AutoReadOnly Hidden  ; aFootJob: pos_foot is moving around partner's pp
int Property t08 = 8 AutoReadOnly Hidden  ; aBoobJob: pos_boob is moving around partner's pp

;--> Mouth doing something
int Property t09 = 9 AutoReadOnly Hidden  ; bKissing: pos_face is in front of partner's face
int Property t10 = 10 AutoReadOnly Hidden ; aSuckingToes: pos_face is in front of partner's toes
int Property t11 = 11 AutoReadOnly Hidden ; pFacial: pos_face is in front of partner's pp
int Property t12 = 12 AutoReadOnly Hidden ; aOral: pos_mouth is licking/sucking partner's crotch
int Property t13 = 13 AutoReadOnly Hidden ; aLickingShaft: pos_mouth is licking shaft of partner's pp
int Property t14 = 14 AutoReadOnly Hidden ; aDeepthroat: pos_mouth is deep-throating partner's pp

;--> Penile penetration (actor receiving)
int Property t15 = 15 AutoReadOnly Hidden ; pVaginal: pos_vag is being penetrated by partner's pp
int Property t16 = 16 AutoReadOnly Hidden ; pAnal: pos_anus is being penetrated by partner's pp

;--> PP doing something (actor's penis)
int Property t17 = 17 AutoReadOnly Hidden ; aFacial: pos_pp is in front of partner's face
int Property t18 = 18 AutoReadOnly Hidden ; aGrinding: pos_crotch is grinding against partner's body
int Property t19 = 19 AutoReadOnly Hidden ; pHandJob: pos_pp is being pleasured by partner's hands
int Property t20 = 20 AutoReadOnly Hidden ; pFootJob: pos_pp is being pleasured by partner's feet
int Property t21 = 21 AutoReadOnly Hidden ; pBoobJob: pos_pp is being pleasured by partner's boobs
int Property t22 = 22 AutoReadOnly Hidden ; pLickingShaft: pos_pp's shaft is being licked by partner's tongue
int Property t23 = 23 AutoReadOnly Hidden ; pOral: pos_crotch is being licked/sucked by partner's mouth
int Property t24 = 24 AutoReadOnly Hidden ; pDeepthroat: pos_pp is deep inside partner's mouth
int Property t25 = 25 AutoReadOnly Hidden ; aSkullfuck: pos_pp is penetrating partner's head
int Property t26 = 26 AutoReadOnly Hidden ; aVaginal: pos_pp is penetrating partner's vagina
int Property t27 = 27 AutoReadOnly Hidden ; aAnal: pos_pp is penetrating partner's anus

int Property SUPPORTED_INTER_COUNT = 28 AutoReadOnly Hidden

; Legacy CType constants (deprecated, kept for backward compatibility only)
int Property CTYPE_ANY = -1 AutoReadOnly
int Property CTYPE_Vaginal = 1 AutoReadOnly
int Property CTYPE_Anal = 2 AutoReadOnly
int Property CTYPE_Oral = 3 AutoReadOnly
int Property CTYPE_Grinding = 4 AutoReadOnly
int Property CTYPE_Deepthroat = 5 AutoReadOnly
int Property CTYPE_Skullfuck = 6 AutoReadOnly
int Property CTYPE_LickingShaft = 7 AutoReadOnly
int Property CTYPE_FootJob = 8 AutoReadOnly
int Property CTYPE_HandJob = 9 AutoReadOnly
int Property CTYPE_Kissing = 10 AutoReadOnly
int Property CTYPE_Facial = 11 AutoReadOnly
int Property CTYPE_AnimObjFace = 12 AutoReadOnly
int Property CTYPE_SuckingToes = 13 AutoReadOnly

; =============================================================== ;
; === INTERACTION DETECTION API                                 === ;
; =============================================================== ;
;/
    Interaction detection via standardized InterType system.
    All detection logic is handled in C++ through the dispatch layer.
    Falls back to position tags when collision data unavailable.
/;

; If collision related data is currently available or not
bool Function IsInteractionRegistered()
EndFunction

; Get a list of all interaction types between two actors
; Returns InterType values (0-27)
int[] Function GetInteractionTypes(Actor akPosition, Actor akPartner)
EndFunction

; Check if specific interaction type exists between actors
bool Function HasInteractionType(int aiType, Actor akPosition, Actor akPartner)
EndFunction

; Get first partner interacting with position by type
Actor Function GetPartnerByType(Actor akPosition, int aiType)
EndFunction

; Get all partners interacting with position by type
Actor[] Function GetPartnersByType(Actor akPosition, int aiType)
EndFunction

; Get first position that partner interacts with by type (reverse direction)
Actor Function GetPartnerByTypeRev(Actor akPartner, int aiType)
EndFunction

; Get all positions that partner interacts with by type (reverse direction)
Actor[] Function GetPartnersByTypeRev(Actor akPartner, int aiType)
EndFunction

; Get velocity of specific interaction
float Function GetVelocity(Actor akPosition, Actor akPartner, int aiType)
EndFunction

; =============================================================== ;
; === INTERACTION FLAGS API                                     === ;
; =============================================================== ;
;/
    Detailed interaction state for an actor at any given moment.
    Returns array of 28 bools indexed by InterType value.
    Updated per-frame, safe for frequent calls.
/;

; Get all interaction flags for an actor (28 bools)
bool[] Function GetCurrentInteractionFlags(Actor akPosition)
EndFunction

; Check if actor has specific interaction flag
bool Function HasCurrentInteractionFlag(Actor akPosition, int aiType)
EndFunction

; Check if actor has all specified interaction flags
bool Function HasCurrentInteractionFlagsAll(Actor akPosition, int[] aiTypes)
EndFunction

; Check if actor has any of specified interaction flags
bool Function HasCurrentInteractionFlagsAny(Actor akPosition, int[] aiTypes)
EndFunction

; Get string representation of active interactions
string Function GetCurrentInteractionString(Actor akPosition)
EndFunction

; Get string array of active interactions
string[] Function GetCurrentInteractionStringA(Actor akPosition)
EndFunction

; =============================================================== ;
; === TAG-BASED FALLBACK API                                    === ;
; =============================================================== ;
;/
    Tag-based detection fallback when collision data unavailable.
    Uses stage/position tags from registry to determine interactions.
/;

bool Function HasTag(string asTag)
EndFunction

bool Function HasSceneTag(string asTag)
EndFunction

bool Function HasStageTag(string asTag)
EndFunction

; Convenience tag checks
bool Function IsVaginal()
EndFunction

bool Function IsAnal()
EndFunction

bool Function IsOral()
EndFunction

; =============================================================== ;
; === COMPLEX INTERACTION DETECTION                             === ;
; =============================================================== ;

bool Function IsVaginalComplex(Actor akPosition)
EndFunction

bool Function IsAnalComplex(Actor akPosition)
EndFunction

bool Function IsOralComplex(Actor akPosition)
EndFunction

; =============================================================== ;
; === LEGACY COMPATIBILITY WRAPPERS                             === ;
; =============================================================== ;
;/
    Legacy CType wrappers - internally convert to InterType.
    Deprecated: Use InterType values directly instead.
/;

; Legacy wrapper - converts CType to InterType internally
bool Function HasCollisionAction(int aiCType, Actor akPosition, Actor akPartner)
    return HasInteractionType(CTypeToInterType(aiCType), akPosition, akPartner)
EndFunction

; Legacy wrapper - converts CType to InterType internally
int[] Function GetCollisionActions(Actor akPosition, Actor akPartner)
    int[] result = GetInteractionTypes(akPosition, akPartner)
    ; Convert InterType back to CType for legacy compatibility
    int[] legacyResult = new int[result.Length]
    int i = 0
    While i < result.Length
        legacyResult[i] = InterTypeToCType(result[i])
        i += 1
    EndWhile
    return legacyResult
EndFunction

; Legacy wrapper - converts CType to InterType internally
Actor Function GetPartnerByAction(Actor akPosition, int aiCType)
    return GetPartnerByType(akPosition, CTypeToInterType(aiCType))
EndFunction

; Legacy wrapper - converts CType to InterType internally
Actor[] Function GetPartnersByAction(Actor akPosition, int aiCType)
    return GetPartnersByType(akPosition, CTypeToInterType(aiCType))
EndFunction

; Legacy wrapper - converts CType to InterType internally
Actor Function GetPartnerByActionRev(Actor akPartner, int aiCType)
    return GetPartnerByTypeRev(akPartner, CTypeToInterType(aiCType))
EndFunction

; Legacy wrapper - converts CType to InterType internally
Actor[] Function GetPartnersByActionRev(Actor akPartner, int aiCType)
    return GetPartnersByTypeRev(akPartner, CTypeToInterType(aiCType))
EndFunction

; Legacy wrapper - converts CType to InterType internally
float Function GetActionVelocity(Actor akPosition, Actor akPartner, int aiCType)
    return GetVelocity(akPosition, akPartner, CTypeToInterType(aiCType))
EndFunction

; =============================================================== ;
; === CONVERSION UTILITIES                                      === ;
; =============================================================== ;
;/
    Convert between legacy CType and standardized InterType.
    Internal use - prefer InterType directly in new code.
/;

; Convert legacy CType to InterType (internal helper)
int Function CTypeToInterType(int aiCType)
    If aiCType == CTYPE_Vaginal
        return t15  ; pVaginal
    ElseIf aiCType == CTYPE_Anal
        return t16  ; pAnal
    ElseIf aiCType == CTYPE_Oral
        return t23  ; pOral
    ElseIf aiCType == CTYPE_Grinding
        return t04  ; pGrinding
    ElseIf aiCType == CTYPE_Deepthroat
        return t24  ; pDeepthroat
    ElseIf aiCType == CTYPE_Skullfuck
        return t05  ; pSkullfuck
    ElseIf aiCType == CTYPE_LickingShaft
        return t22  ; pLickingShaft
    ElseIf aiCType == CTYPE_FootJob
        return t20  ; pFootJob
    ElseIf aiCType == CTYPE_HandJob
        return t19  ; pHandJob
    ElseIf aiCType == CTYPE_Kissing
        return t09  ; bKissing
    ElseIf aiCType == CTYPE_Facial
        return t11  ; pFacial
    ElseIf aiCType == CTYPE_AnimObjFace
        return t02  ; pAnimObjFace
    ElseIf aiCType == CTYPE_SuckingToes
        return t03  ; pSuckingToes
    EndIf
    return -1  ; No mapping
EndFunction

; Convert InterType to legacy CType (internal helper, deprecated)
int Function InterTypeToCType(int aiInterType)
    If aiInterType == t15  ; pVaginal
        return CTYPE_Vaginal
    ElseIf aiInterType == t16  ; pAnal
        return CTYPE_Anal
    ElseIf aiInterType == t23  ; pOral
        return CTYPE_Oral
    ElseIf aiInterType == t04  ; pGrinding
        return CTYPE_Grinding
    ElseIf aiInterType == t24  ; pDeepthroat
        return CTYPE_Deepthroat
    ElseIf aiInterType == t05  ; pSkullfuck
        return CTYPE_Skullfuck
    ElseIf aiInterType == t22  ; pLickingShaft
        return CTYPE_LickingShaft
    ElseIf aiInterType == t20  ; pFootJob
        return CTYPE_FootJob
    ElseIf aiInterType == t19  ; pHandJob
        return CTYPE_HandJob
    ElseIf aiInterType == t09  ; bKissing
        return CTYPE_Kissing
    ElseIf aiInterType == t11  ; pFacial
        return CTYPE_Facial
    ElseIf aiInterType == t02  ; pAnimObjFace
        return CTYPE_AnimObjFace
    ElseIf aiInterType == t03  ; pSuckingToes
        return CTYPE_SuckingToes
    EndIf
    return -1  ; No mapping
EndFunction

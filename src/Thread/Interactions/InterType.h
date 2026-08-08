#pragma once

#include "Registry/Define/Tags.h"
#include <array>
#include <string_view>
#include <optional>

namespace Thread::Interactions
{
    // Obfuscated InterType enum - standardized replacement for CType
    // Range: 0-27 (SUPPORTED_INTER_COUNT = 28)
    enum class InterType : int
    {
        // Stimulation types (actor receiving non-penile stimulation)
        pStimulation = 0,      // pos_crotch is being fingered, fisted, or toys_inserted
        aAnimObjFace = 1,      // pos_anim_obj is in front of partner's face
        pAnimObjFace = 2,      // pos_face is in front of partner's anim_obj

        // Body receiving/doing something
        pSuckingToes = 3,      // pos_toes are closer to partner's mouth
        pGrinding = 4,         // pos_body is being grinded against by partner's crotch
        pSkullfuck = 5,        // pos_head is being penetrated by partner's pp
        aHandJob = 6,          // pos_hand is moving around partner's pp
        aFootJob = 7,          // pos_foot is moving around partner's pp
        aBoobJob = 8,          // pos_boob is moving around partner's pp

        // Mouth doing something
        bKissing = 9,          // pos_face is in front of partner's face
        aSuckingToes = 10,     // pos_face is in front of partner's toes
        pFacial = 11,          // pos_face is in front of partner's pp
        aOral = 12,            // pos_mouth is licking/sucking partner's crotch
        aLickingShaft = 13,    // pos_mouth is licking shaft of partner's pp
        aDeepthroat = 14,      // pos_mouth is deep-throating partner's pp

        // Penile penetration (actor receiving)
        pVaginal = 15,         // pos_vag is being penetrated by partner's pp
        pAnal = 16,            // pos_anus is being penetrated by partner's pp

        // PP doing something (actor's penis)
        aFacial = 17,          // pos_pp is in front of partner's face
        aGrinding = 18,        // pos_crotch is grinding against partner's body
        pHandJob = 19,         // pos_pp is being pleasured by partner's hands
        pFootJob = 20,         // pos_pp is being pleasured by partner's feet
        pBoobJob = 21,         // pos_pp is being pleasured by partner's boobs
        pLickingShaft = 22,    // pos_pp's shaft is being licked by partner's tongue
        pOral = 23,            // pos_crotch is being licked/sucked by partner's mouth
        pDeepthroat = 24,      // pos_pp is deep inside partner's mouth
        aSkullfuck = 25,       // pos_pp is penetrating partner's head
        aVaginal = 26,         // pos_pp is penetrating partner's vagina
        aAnal = 27,            // pos_pp is penetrating partner's anus

        SUPPORTED_INTER_COUNT = 28
    };

    // Legacy CType mapping (for backward compatibility during transition)
    enum class CType : int
    {
        CTYPE_ANY = -1,
        CTYPE_Vaginal = 1,
        CTYPE_Anal = 2,
        CTYPE_Oral = 3,
        CTYPE_Grinding = 4,
        CTYPE_Deepthroat = 5,
        CTYPE_Skullfuck = 6,
        CTYPE_LickingShaft = 7,
        CTYPE_FootJob = 8,
        CTYPE_HandJob = 9,
        CTYPE_Kissing = 10,
        CTYPE_Facial = 11,
        CTYPE_AnimObjFace = 12,
        CTYPE_SuckingToes = 13
    };

    constexpr int SUPPORTED_INTER_COUNT = 28;

    // Lookup table: InterType -> string name (obfuscated-friendly)
    [[nodiscard]] std::string_view GetInterTypeName(InterType type);

    // Lookup table: InterType -> corresponding tag for fallback detection
    [[nodiscard]] Registry::Tag GetCorrespondingTag(InterType type);

    // Convert legacy CType to InterType (returns nullopt if no mapping)
    [[nodiscard]] std::optional<InterType> CTypeToInterType(CType ctype);

    // Convert NiNode NiType to InterType where applicable
    // Note: NiType covers subset (penetration/grinding/kissing types)
    [[nodiscard]] std::optional<InterType> NiTypeToInterType(int niTypeValue);

    // Check if InterType represents penetration (vaginal/anal/oral receiving)
    [[nodiscard]] bool IsPenetrationReceiving(InterType type);

    // Check if InterType represents penetration (vaginal/anal/oral giving)
    [[nodiscard]] bool IsPenetrationGiving(InterType type);

    // Check if InterType represents oral activity
    [[nodiscard]] bool IsOralActivity(InterType type);

    // Initialize lookup tables (called once at startup)
    void Initialize();

}  // namespace Thread::Interactions

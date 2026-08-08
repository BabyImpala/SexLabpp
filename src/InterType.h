#pragma once
#include "skse64/GameRTTI.h"
#include "skse64/GameObjects.h"
#include "skse64/GameReferences.h"
#include "skse64/NiTypes.h"
#include <unordered_map>
#include <string>

// Obfuscated InterType Definitions
namespace InterType {
    enum Type : int {
        t00_None = 0,
        t01_Face = 1,
        t02_FacePassive = 2,
        t03_OR = 3,       // Oral
        t04_GR = 4,       // Grinding
        t05_VG = 5,       // Vaginal
        t06_AG = 6,       // Anal
        t07_HN = 7,       // Hand
        t08_FT = 8,       // Foot
        t09_TP = 9,       // Triple
        t10_PN = 10,      // Penetration
        t11_MS = 11,      // Misc
        t12_Max = 12
    };
}

// Legacy/CType mapping for backward compatibility if needed internally
namespace CTypeLegacy {
    enum Type : int {
        Any = -1,
        Oral = 3,
        Grinding = 4
    };
}

class InteractionDetector {
public:
    static InteractionDetector* GetSingleton();

    // Main dispatch: Returns InterType based on ML or Legacy logic
    int DetectInteractionType(Actor* actorA, Actor* actorB, TESObjectREFR* posRef, bool useML);

    // Tag-based fallback (matches Papyrus sslThreadModel behavior)
    int GuessByPositionTags(TESObjectREFR* posRef);

    // Velocity/Movement helper
    float CalculateVelocity(Actor* actor);

private:
    InteractionDetector() {}
    ~InteractionDetector() {}

    // Caching maps for performance (similar to enjoyment map)
    std::unordered_map<RE::FormID, int> _tagCache;
    
    int RunMLDetection(Actor* actorA, Actor* actorB);
    int RunLegacyDetection(Actor* actorA, Actor* actorB, TESObjectREFR* posRef);
};

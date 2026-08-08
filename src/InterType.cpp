#include "InterType.h"
#include "Thread.h"
#include <algorithm>

static InteractionDetector* g_singleton = nullptr;

InteractionDetector* InteractionDetector::GetSingleton() {
    if (!g_singleton) {
        g_singleton = new InteractionDetector();
    }
    return g_singleton;
}

int InteractionDetector::DetectInteractionType(Actor* actorA, Actor* actorB, TESObjectREFR* posRef, bool useML) {
    if (useML) {
        return RunMLDetection(actorA, actorB);
    }
    return RunLegacyDetection(actorA, actorB, posRef);
}

int InteractionDetector::RunMLDetection(Actor* actorA, Actor* actorB) {
    // Placeholder: Insert ML model inference logic here
    // Returns InterType enum value
    return InterType::t00_None;
}

int InteractionDetector::RunLegacyDetection(Actor* actorA, Actor* actorB, TESObjectREFR* posRef) {
    // Fallback to tag guessing if legacy logic fails or is inconclusive
    int guessed = GuessByPositionTags(posRef);
    if (guessed != InterType::t00_None) {
        return guessed;
    }

    // Basic legacy heuristic (placeholder for actual collision/velocity logic)
    float vel = CalculateVelocity(actorA);
    if (vel > 100.0f) {
        return InterType::t04_GR; // High velocity often implies grinding
    }

    return InterType::t00_None;
}

int InteractionDetector::GuessByPositionTags(TESObjectREFR* posRef) {
    if (!posRef) return InterType::t00_None;

    RE::FormID formID = posRef->formID;
    
    // Check cache first
    auto it = _tagCache.find(formID);
    if (it != _tagCache.end()) {
        return it->second;
    }

    // Placeholder: Actual implementation would read keywords/tags from Form
    // Matching Papyrus sslThreadModel behavior
    // Example pseudo-logic:
    // if (posRef->HasKeyword("OralTag")) result = t03_OR;
    // if (posRef->HasKeyword("GrindTag")) result = t04_GR;
    
    int result = InterType::t00_None;
    
    // Cache result
    _tagCache[formID] = result;
    return result;
}

float InteractionDetector::CalculateVelocity(Actor* actor) {
    if (!actor) return 0.0f;
    
    // Placeholder: Access NiNode velocity from actor
    // NiPoint3 velocity = actor->GetVelocity();
    // return velocity.Length();
    
    return 0.0f;
}

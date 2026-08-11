#include "Interaction.h"

#include "Registry/Define/Animation.h"
#include "Thread/Thread.h"

using namespace Thread::NiNode;
using namespace Thread::LegacyNiNode;

namespace Thread::Interaction
{
    // ============ ML NiInstance ============ //

    static NiType::Type ToNiType(CType ct)
    {
        return static_cast<NiType::Type>(static_cast<int32_t>(ct));
    }

    static std::vector<int32_t> GetCollisionActionsML(NiNode::NiInstance* ni, RE::Actor* a_actor, RE::Actor* a_partner)
    {
        const uint32_t idxA = a_actor ? a_actor->formID : 0;
        const uint32_t idxB = a_partner ? a_partner->formID : 0;
        std::vector<int32_t> result;
        for (const auto* inter : ni->GetInteractions(idxA, idxB, ToNiType(CType::None))) {
            const auto ct = static_cast<int32_t>(inter->GetType());
            if (ct > 0 && ct < kCTypeCount)
                for (int32_t it : InterTypesWithActorAsIdxA()[ct])
                    result.push_back(it);
        }
        for (const auto* inter : ni->GetInteractions(idxB, idxA, ToNiType(CType::None))) {
            const auto ct = static_cast<int32_t>(inter->GetType());
            if (ct > 0 && ct < kCTypeCount)
                for (int32_t it : InterTypesWithPartnerAsIdxA()[ct])
                    result.push_back(it);
        }
        return result;
    }

    static bool HasCollisionActionML(NiNode::NiInstance* ni, RE::Actor* a_actor, RE::Actor* a_partner, int32_t interType)
    {
        const auto& entry = kInterTypeTable[interType];
        const uint32_t idxA = a_actor ? a_actor->formID : 0;
        const uint32_t idxB = a_partner ? a_partner->formID : 0;
        const auto niType = ToNiType(entry.ctype);
        const auto interactions = entry.swapped
            ? ni->GetInteractions(idxB, idxA, niType)
            : ni->GetInteractions(idxA, idxB, niType);
        return !interactions.empty();
    }

    static std::vector<RE::Actor*> GetPartnersByActionML(NiNode::NiInstance* ni, RE::Actor* a_actor,  int32_t interType)
    {
        const auto& entry = kInterTypeTable[interType];
        const uint32_t idx = a_actor ? a_actor->formID : 0;
        const auto niType = ToNiType(entry.ctype);

        return entry.swapped
            ? ni->GetInteractionPartnersRev(idx, niType)
            : ni->GetInteractionPartners(idx, niType);
    }

    static float GetActionVelocityML(NiNode::NiInstance* ni, RE::Actor* a_actor, RE::Actor* a_partner, int32_t interType)
    {
        const auto& entry = kInterTypeTable[interType];
        const uint32_t idxA = a_actor ? a_actor->formID : 0;
        const uint32_t idxB = a_partner ? a_partner->formID : 0;
        const auto niType = ToNiType(entry.ctype);
        const auto interactions = entry.swapped
            ? ni->GetInteractions(idxB, idxA, niType)
            : ni->GetInteractions(idxA, idxB, niType);
        return interactions.empty() ? 0.0f : interactions.front()->velocity;
    }

    // ============ Legacy NiInstance ============ //

    static std::vector<int32_t> GetCollisionActionsLegacy(LegacyNiNode::NiInstance* ni, RE::Actor* a_actor, RE::Actor* a_partner)
    {
        const uint32_t idxA = a_actor ? a_actor->formID : 0;
        const uint32_t idxB = a_partner ? a_partner->formID : 0;
        std::vector<int32_t> result;
        ni->VisitPositions([&](auto& pos) {
            // Case 1: CType's definintional role performed by a_actor (covers all swapped == false entries)
            if (pos.actor->formID == idxA) {
                for (const auto& type : pos.interactions) {
                    const auto ct = static_cast<int32_t>(type.action);
                    if (ct <= 0 || ct >= kCTypeCount)
                        continue;
                    if (a_partner && (!type.partner || type.partner->formID != idxB))
                        continue;
                    for (int32_t it : InterTypesWithActorAsIdxA()[ct])
                        result.push_back(it);
                }
            }
            // Case 2: CType's definintional role performed by other position (covers all swapped == true entries)
            // Restrict to a_partner's position if one was given, otherwise check every position.
            if (!a_partner || pos.actor->formID == idxB) {
                for (const auto& type : pos.interactions) {
                    if (!type.partner || type.partner->formID != idxA)
                        continue;
                    const auto ct = static_cast<int32_t>(type.action);
                    if (ct <= 0 || ct >= kCTypeCount)
                        continue;
                    for (int32_t it : InterTypesWithPartnerAsIdxA()[ct])
                        result.push_back(it);
                }
            }
            return false;
        });
        return result;
    }

    static bool HasCollisionActionLegacy(LegacyNiNode::NiInstance* ni, RE::Actor* a_actor, RE::Actor* a_partner, int32_t interType)
    {
        const auto& entry = kInterTypeTable[interType];
        const auto ct = static_cast<int32_t>(entry.ctype);
        RE::Actor* actorA = entry.swapped ? a_partner : a_actor;
        RE::Actor* actorB = entry.swapped ? a_actor : a_partner;
        bool found = false;
        ni->VisitPositions([&](auto& pos) {
            if (actorA && pos.actor->formID != actorA->formID)
                return false;
            for (const auto& type : pos.interactions) {
                if (static_cast<int32_t>(type.action) != ct)
                    continue;
                if (actorB && (!type.partner || type.partner->formID != actorB->formID))
                    continue;
                found = true;
                return true;
            }
            return false;
        });
        return found;
    }

    static std::vector<RE::Actor*> GetPartnersByActionLegacy(LegacyNiNode::NiInstance* ni, RE::Actor* a_actor, int32_t interType)
    {
        const auto& entry = kInterTypeTable[interType];
        const auto ct = static_cast<int32_t>(entry.ctype);
        std::vector<RE::Actor*> result;
        ni->VisitPositions([&](auto& pos) {
            if (!entry.swapped) {
                if (pos.actor->formID != a_actor->formID)
                    return false;
                for (const auto& type : pos.interactions)
                    if (static_cast<int32_t>(type.action) == ct && type.partner)
                        result.push_back(type.partner.get());
            } else {
                for (const auto& type : pos.interactions)
                    if (static_cast<int32_t>(type.action) == ct && type.partner &&
                        type.partner->formID == a_actor->formID)
                        result.push_back(pos.actor.get());
            }
            return false;
        });
        return result;
    }

    static float GetActionVelocityLegacy(LegacyNiNode::NiInstance* ni, RE::Actor* a_actor, RE::Actor* a_partner, int32_t interType)
    {
        const auto& entry = kInterTypeTable[interType];
        const auto ct = static_cast<int32_t>(entry.ctype);
        RE::Actor* actorA = entry.swapped ? a_partner : a_actor;
        RE::Actor* actorB = entry.swapped ? a_actor : a_partner;
        float ret = 0.0f;
        ni->VisitPositions([&](auto& pos) {
            if (actorA && pos.actor->formID != actorA->formID)
                return false;
            for (const auto& type : pos.interactions) {
                if (static_cast<int32_t>(type.action) != ct)
                    continue;
                if (actorB && (!type.partner || type.partner->formID != actorB->formID))
                    continue;
                ret = type.velocity;
                return true;
            }
            return false;
        });
        return ret;
    }

    // ============ PosTags Fallback ============ //

    static bool ShouldUseTagsFallback(Thread::Instance* instance)
    {
        if (!Settings::bFallbackToTagsForDetection)
            return false;
        const auto* stage = instance->GetActiveStage();
        return stage && stage->tags.HasTag("PosTagged");
    }

    static std::vector<bool> GetInteractionPosTags(Thread::Instance* instance, RE::Actor* a_actor)
    {
        std::vector<bool> flags(kInterTypeCount, false);
        const auto* stage = instance->GetActiveStage();
        if (!stage)
            return flags;
        const auto& positions = instance->GetActors();
        const auto it = std::find(positions.begin(), positions.end(), a_actor);
        if (it == positions.end())
            return flags;
        const int32_t idx = static_cast<int32_t>(std::distance(positions.begin(), it));
        if (idx >= static_cast<int32_t>(stage->positions.size()))
            return flags;

        const auto& byName = InterTypeByName();
        for (const auto& tag : stage->positions[idx].tags) {
            if (const auto found = byName.find(tag); found != byName.end())
                flags[found->second] = true;
        }
        return flags;
    }

    // ============ Dispatch Helpers ============ //

    static std::vector<bool> BuildInteractionFlags(Thread::Instance* instance, RE::Actor* a_actor, RE::Actor* a_partner)
    {
        auto flags = [](const std::vector<int32_t>& its) {
            std::vector<bool> f(kInterTypeCount, false);
            for (int32_t it : its)
                if (it >= 0 && it < kInterTypeCount)
                    f[it] = true;
            return f;
        };

        std::vector<bool> interFlags(kInterTypeCount, false);

        if (Settings::bUseLegacyNiType) {
            if (auto* ni = instance->GetNiInstanceLegacy()) {
                interFlags = flags(GetCollisionActionsLegacy(ni, a_actor, a_partner));
            }
        } else {
            if (auto* ni = instance->GetNiInstance()) {
                interFlags = flags(GetCollisionActionsML(ni, a_actor, a_partner));
            }
        }
        bool allFalse = std::find(interFlags.begin(), interFlags.end(), true) == interFlags.end();
        if (allFalse && ShouldUseTagsFallback(instance)) {
            return GetInteractionPosTags(instance, a_actor);
        }
        return interFlags;
    }

    static bool ValidateInterType(int32_t interType, const char* caller)
    {
        if (interType < 0 || interType >= kInterTypeCount) {
            logger::warn("{}: interType {} out of range", caller, interType);
            return false;
        }
        return true;
    }

    // ============ Public API ============ //

    std::vector<bool> GetInteractionFlagsImpl(Thread::Instance* instance, RE::Actor* a_actor, RE::Actor* a_partner)
    {
        return BuildInteractionFlags(instance, a_actor, a_partner);
    }

    std::vector<int32_t> GetActiveInterTypesImpl(Thread::Instance* instance, RE::Actor* a_actor, RE::Actor* a_partner)
    {
        if (!a_actor) {
            logger::warn("{}: actor is none", __func__);
            return {};
        }
        const auto flags = BuildInteractionFlags(instance, a_actor, a_partner);
        std::vector<int32_t> result;
        for (int32_t i = 0; i < kInterTypeCount; ++i)
            if (flags[i])
                result.push_back(i);
        return result;
    }

    bool HasActiveInteractionImpl(Thread::Instance* instance, RE::Actor* a_actor, RE::Actor* a_partner, int32_t interType)
    {
        bool ret = false;
        if (!a_actor) {
            logger::warn("{}: actor is none", __func__);
            return ret;
        }
        if (!ValidateInterType(interType, __func__)) {
            return ret;
        }
        const auto& entry = kInterTypeTable[interType];
        if (Settings::bUseLegacyNiType) {
            if (auto* ni = instance->GetNiInstanceLegacy()) {
                ret = entry.supported && HasCollisionActionLegacy(ni, a_actor, a_partner, interType);
            }
        } else {
            if (auto* ni = instance->GetNiInstance()) {
                ret = entry.supported && HasCollisionActionML(ni, a_actor, a_partner, interType);
            }
        }
        if (!ret && ShouldUseTagsFallback(instance)) {
            ret = GetInteractionPosTags(instance, a_actor)[interType];
        }
        return ret;
    }

    bool HasActiveInteractionAllImpl(Thread::Instance* instance, RE::Actor* a_actor, const std::vector<int32_t>& interTypes)
    {
        if (!a_actor) {
            logger::warn("{}: actor is none", __func__);
            return false;
        }
        if (interTypes.empty())
            return false;
        const auto flags = BuildInteractionFlags(instance, a_actor, nullptr);
        return std::ranges::all_of(interTypes, [&](int32_t t) {
            return t >= 0 && t < kInterTypeCount && flags[t];
        });
    }

    bool HasActiveInteractionAnyImpl(Thread::Instance* instance, RE::Actor* a_actor, const std::vector<int32_t>& interTypes)
    {
        if (!a_actor) {
            logger::warn("{}: actor is none", __func__);
            return false;
        }
        if (interTypes.empty())
            return false;
        const auto flags = BuildInteractionFlags(instance, a_actor, nullptr);
        return std::ranges::any_of(interTypes, [&](int32_t t) {
            return t >= 0 && t < kInterTypeCount && flags[t];
        });
    }

    std::vector<RE::Actor*> GetPartnersByInteractionTypeImpl(Thread::Instance* instance, RE::Actor* a_actor, int32_t interType)
    {
        if (!a_actor) {
            logger::warn("{}: actor is none", __func__);
            return {};
        }
        if (!ValidateInterType(interType, __func__))
            return {};
        const auto& entry = kInterTypeTable[interType];
        if (entry.supported) {
            if (Settings::bUseLegacyNiType) {
                if (auto* ni = instance->GetNiInstanceLegacy()) {
                    return GetPartnersByActionLegacy(ni, a_actor, interType);
                }
            } else {
                if (auto* ni = instance->GetNiInstance()) {
                    return GetPartnersByActionML(ni, a_actor, interType);
                }
            }
        }
        if (ShouldUseTagsFallback(instance)) {
            const auto complement = kInterTypeTable[interType].complement;
            if (complement < 0 || complement >= kInterTypeCount)
                return {};
            std::vector<RE::Actor*> result;
            for (auto* other : instance->GetActors()) {
                if (!other || other->formID == a_actor->formID)
                    continue;
                if (GetInteractionPosTags(instance, other)[complement])
                    result.push_back(other);
            }
            return result;
        }
        return {};
    }

    RE::Actor* GetPartnerByInteractionTypeImpl(Thread::Instance* instance, RE::Actor* a_actor, int32_t interType)
    {
        const auto result = GetPartnersByInteractionTypeImpl(instance, a_actor, interType);
        return result.empty() ? nullptr : result.front();
    }

    float GetInteractionVelocityImpl(Thread::Instance* instance, RE::Actor* a_actor, RE::Actor* a_partner, int32_t interType)
    {
        if (!a_actor) {
            logger::warn("{}: actor is none", __func__);
            return 0.0f;
        }
        if (!ValidateInterType(interType, __func__)) {
            return 0.0f;
        }
        if (!kInterTypeTable[interType].supported)
            return 0.0f;
        if (Settings::bUseLegacyNiType) {
            if (auto* ni = instance->GetNiInstanceLegacy()) {
                return GetActionVelocityLegacy(ni, a_actor, a_partner, interType);
            }
        } else {
            if (auto* ni = instance->GetNiInstance()) {
                return GetActionVelocityML(ni, a_actor, a_partner, interType);
            }
        }
        return 0.0f;
    }

    std::vector<RE::BSFixedString> GetInteractionStringArrayImpl(Thread::Instance* instance, RE::Actor* a_actor)
    {
        std::vector<RE::BSFixedString> result;
        const auto flags = BuildInteractionFlags(instance, a_actor, nullptr);
        for (int32_t i = 0; i < kInterTypeCount; ++i)
            if (flags[i])
                result.emplace_back(kInterTypeTable[i].name.data());
        return result;
    }

    std::string GetInteractionStringImpl(Thread::Instance* instance, RE::Actor* a_actor)
    {
        const auto names = GetInteractionStringArrayImpl(instance, a_actor);
        std::string ret;
        for (const auto& name : names) {
            if (!ret.empty())
                ret += ',';
            ret += name.c_str();
        }
        return ret;
    }

}  // namespace Thread::Interaction

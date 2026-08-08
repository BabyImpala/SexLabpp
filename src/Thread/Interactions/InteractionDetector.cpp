#include "InteractionDetector.h"
#include "Thread/Thread.h"
#include "Registry/Library.h"
#include <logger.h>

namespace Thread::Interactions
{
    void InteractionDetector::RegisterNiInstance(RE::FormID threadFormID, NiNode::NiInstance* niInstance)
    {
        std::unique_lock lock(m_mutex);
        auto& data = m_threadData[threadFormID];
        if (niInstance) {
            // Find the shared_ptr in Thread::Instance and store weak reference
            // This is a simplified approach - actual implementation would need to get 
            // the shared_ptr from Thread::Instance
            data.useLegacy = false;
        }
    }

    void InteractionDetector::RegisterNiInstanceLegacy(RE::FormID threadFormID, 
        LegacyNiNode::NiInstance* niInstance)
    {
        std::unique_lock lock(m_mutex);
        auto& data = m_threadData[threadFormID];
        if (niInstance) {
            data.useLegacy = true;
        }
    }

    void InteractionDetector::Unregister(RE::FormID threadFormID)
    {
        std::unique_lock lock(m_mutex);
        m_threadData.erase(threadFormID);
    }

    bool InteractionDetector::IsRegistered(RE::FormID threadFormID) const
    {
        std::shared_lock lock(m_mutex);
        auto it = m_threadData.find(threadFormID);
        return it != m_threadData.end() && 
               (!it->second.niInstance.expired() || !it->second.niInstanceLegacy.expired());
    }

    std::vector<int> InteractionDetector::GetInteractionTypes(RE::FormID threadFormID,
        RE::Actor* position, RE::Actor* partner) const
    {
        std::shared_lock lock(m_mutex);
        auto it = m_threadData.find(threadFormID);
        if (it == m_threadData.end()) {
            return {};
        }

        if (!it->second.useLegacy) {
            if (auto niInst = it->second.niInstance.lock()) {
                return GetInteractionTypesML(niInst.get(), position, partner);
            }
        } else {
            if (auto niInst = it->second.niInstanceLegacy.lock()) {
                return GetInteractionTypesLegacy(niInst.get(), position, partner);
            }
        }

        // Fallback to tag-based detection if enabled
        if (m_useTagFallback) {
            auto instance = Thread::Instance::GetInstance(
                RE::TESQuest::GetByID(threadFormID));
            if (instance) {
                auto stage = instance->GetActiveStage();
                if (stage) {
                    return GetInteractionTypesByTags(position, partner, stage);
                }
            }
        }

        return {};
    }

    std::vector<int> InteractionDetector::GetInteractionTypesML(NiNode::NiInstance* niInstance,
        RE::Actor* position, RE::Actor* partner) const
    {
        if (!niInstance) return {};

        const auto idxA = position ? position->formID : 0;
        const auto idxB = partner ? partner->formID : 0;
        
        // Get interactions from NiNode system (returns NiType interactions)
        const auto interactions = niInstance->GetInteractions(idxA, idxB, NiNode::NiType::Type::None);
        
        std::vector<int> result;
        result.reserve(interactions.size());
        
        for (const auto& interaction : interactions) {
            // Convert NiType to InterType
            auto niTypeValue = static_cast<int>(interaction->GetType());
            if (auto interType = NiTypeToInterType(niTypeValue)) {
                result.push_back(static_cast<int>(*interType));
            }
        }
        
        return result;
    }

    std::vector<int> InteractionDetector::GetInteractionTypesLegacy(LegacyNiNode::NiInstance* niInstance,
        RE::Actor* position, RE::Actor* partner) const
    {
        if (!niInstance) return {};

        std::vector<int> result;
        
        niInstance->VisitPositions([&](auto& p) {
            if (position && p.actor->formID != position->formID)
                return false;
                
            for (auto&& type : p.interactions) {
                if (partner && type.partner->formID != partner->formID)
                    continue;
                    
                // Legacy uses action enum directly, convert to InterType
                // The legacy action values should map to our InterType enum
                result.push_back(static_cast<int>(type.action));
            }
            return false;
        });
        
        return result;
    }

    std::vector<int> InteractionDetector::GetInteractionTypesByTags(RE::Actor* position,
        RE::Actor* partner, const Registry::Stage* activeStage) const
    {
        if (!activeStage || !position || !partner) return {};

        std::vector<int> result;
        
        // Check stage tags for interaction types
        const auto& tags = activeStage->tags;
        
        // Map tags to InterType based on actor roles
        // This requires knowing which actor is in which position role
        // Simplified implementation - actual would need position info
        
        if (tags.HasTag(Registry::Tag::Vaginal)) {
            result.push_back(static_cast<int>(InterType::pVaginal));
        }
        if (tags.HasTag(Registry::Tag::Anal)) {
            result.push_back(static_cast<int>(InterType::pAnal));
        }
        if (tags.HasTag(Registry::Tag::Oral)) {
            result.push_back(static_cast<int>(InterType::pOral));
        }
        if (tags.HasTag(Registry::Tag::Grinding)) {
            result.push_back(static_cast<int>(InterType::pGrinding));
        }
        if (tags.HasTag(Registry::Tag::Kissing)) {
            result.push_back(static_cast<int>(InterType::bKissing));
        }
        
        return result;
    }

    bool InteractionDetector::HasInteractionType(RE::FormID threadFormID, int interTypeValue,
        RE::Actor* position, RE::Actor* partner) const
    {
        const auto types = GetInteractionTypes(threadFormID, position, partner);
        return std::find(types.begin(), types.end(), interTypeValue) != types.end();
    }

    RE::Actor* InteractionDetector::GetPartnerByType(RE::FormID threadFormID,
        RE::Actor* position, int interTypeValue) const
    {
        const auto partners = GetPartnersByType(threadFormID, position, interTypeValue);
        return partners.empty() ? nullptr : partners.front();
    }

    std::vector<RE::Actor*> InteractionDetector::GetPartnersByType(RE::FormID threadFormID,
        RE::Actor* position, int interTypeValue) const
    {
        std::shared_lock lock(m_mutex);
        auto it = m_threadData.find(threadFormID);
        if (it == m_threadData.end()) {
            return {};
        }

        std::vector<RE::Actor*> result;
        
        if (!it->second.useLegacy) {
            if (auto niInst = it->second.niInstance.lock()) {
                const auto idxA = position ? position->formID : 0;
                const auto niType = /* convert InterType to NiType */ NiNode::NiType::Type::None;
                auto partners = niInst->GetInteractionPartners(idxA, niType);
                // Filter by specific interType if needed
                result = partners;
            }
        } else {
            if (auto niInst = it->second.niInstanceLegacy.lock()) {
                niInst->VisitPositions([&](auto& p) {
                    if (position && p.actor->formID != position->formID)
                        return false;
                    for (auto&& type : p.interactions) {
                        if (static_cast<int>(type.action) == interTypeValue) {
                            result.push_back(type.partner.get());
                        }
                    }
                    return false;
                });
            }
        }
        
        return result;
    }

    RE::Actor* InteractionDetector::GetPartnerByTypeRev(RE::FormID threadFormID,
        RE::Actor* partner, int interTypeValue) const
    {
        const auto partners = GetPartnersByTypeRev(threadFormID, partner, interTypeValue);
        return partners.empty() ? nullptr : partners.front();
    }

    std::vector<RE::Actor*> InteractionDetector::GetPartnersByTypeRev(RE::FormID threadFormID,
        RE::Actor* partner, int interTypeValue) const
    {
        std::shared_lock lock(m_mutex);
        auto it = m_threadData.find(threadFormID);
        if (it == m_threadData.end()) {
            return {};
        }

        std::vector<RE::Actor*> result;
        
        if (!it->second.useLegacy) {
            if (auto niInst = it->second.niInstance.lock()) {
                const auto idxB = partner ? partner->formID : 0;
                const auto niType = /* convert InterType to NiType */ NiNode::NiType::Type::None;
                result = niInst->GetInteractionPartnersRev(idxB, niType);
            }
        } else {
            if (auto niInst = it->second.niInstanceLegacy.lock()) {
                niInst->VisitPositions([&](auto& p) {
                    for (auto&& type : p.interactions) {
                        if (partner && partner->formID == type.partner->formID) {
                            if (interTypeValue == -1 || interTypeValue == static_cast<int>(type.action)) {
                                result.push_back(p.actor.get());
                            }
                            break;
                        }
                    }
                    return false;
                });
            }
        }
        
        return result;
    }

    float InteractionDetector::GetActionVelocity(RE::FormID threadFormID,
        RE::Actor* position, RE::Actor* partner, int interTypeValue) const
    {
        std::shared_lock lock(m_mutex);
        auto it = m_threadData.find(threadFormID);
        if (it == m_threadData.end()) {
            return 0.0f;
        }

        if (!it->second.useLegacy) {
            if (auto niInst = it->second.niInstance.lock()) {
                const auto idxA = position ? position->formID : 0;
                const auto idxB = partner ? partner->formID : 0;
                const auto niType = /* convert InterType to NiType */ NiNode::NiType::Type::None;
                const auto interactions = niInst->GetInteractions(idxA, idxB, niType);
                if (!interactions.empty()) {
                    return interactions.front()->velocity;
                }
            }
        } else {
            if (auto niInst = it->second.niInstanceLegacy.lock()) {
                float velocity = 0.0f;
                niInst->VisitPositions([&](auto& p) {
                    if (position && p.actor->formID != position->formID)
                        return false;
                    for (auto&& type : p.interactions) {
                        if (partner && partner->formID != type.partner->formID)
                            continue;
                        if (static_cast<int>(type.action) == interTypeValue) {
                            velocity = type.velocity;
                            return true;
                        }
                    }
                    return false;
                });
                return velocity;
            }
        }
        
        return 0.0f;
    }

    std::array<bool, SUPPORTED_INTER_COUNT> InteractionDetector::GetCurrentInteractionFlags(
        RE::FormID threadFormID, RE::Actor* position) const
    {
        std::array<bool, SUPPORTED_INTER_COUNT> flags{};
        flags.fill(false);
        
        if (!position) return flags;
        
        // Get all partners and check interactions
        std::shared_lock lock(m_mutex);
        auto it = m_threadData.find(threadFormID);
        if (it == m_threadData.end()) {
            return flags;
        }
        
        // Collect all interaction types for this position
        std::unordered_set<int> detectedTypes;
        
        if (!it->second.useLegacy) {
            if (auto niInst = it->second.niInstance.lock()) {
                const auto idxA = position->formID;
                // Iterate through all possible partners
                niInst->VisitPositions([&](auto& p) {
                    if (p.actor->formID != idxA)
                        return false;
                    for (auto&& interaction : p.interactions) {
                        auto niTypeValue = static_cast<int>(interaction.action);
                        if (auto interType = NiTypeToInterType(niTypeValue)) {
                            detectedTypes.insert(static_cast<int>(*interType));
                        }
                    }
                    return false;
                });
            }
        } else {
            if (auto niInst = it->second.niInstanceLegacy.lock()) {
                niInst->VisitPositions([&](auto& p) {
                    if (p.actor->formID != position->formID)
                        return false;
                    for (auto&& type : p.interactions) {
                        detectedTypes.insert(static_cast<int>(type.action));
                    }
                    return false;
                });
            }
        }
        
        // Set flags for detected types
        for (int typeVal : detectedTypes) {
            if (typeVal >= 0 && typeVal < SUPPORTED_INTER_COUNT) {
                flags[static_cast<size_t>(typeVal)] = true;
            }
        }
        
        return flags;
    }

    void InteractionDetector::Update(float a_delta)
    {
        std::shared_lock lock(m_mutex);
        for (auto& [formID, data] : m_threadData) {
            // Update hysteresis timers for active interactions
            // This is called per frame to maintain interaction state
            if (!data.useLegacy) {
                if (auto niInst = data.niInstance.lock()) {
                    // ML system handles its own updates
                }
            } else {
                if (auto niInst = data.niInstanceLegacy.lock()) {
                    // Legacy system handles its own updates
                }
            }
        }
    }

}  // namespace Thread::Interactions

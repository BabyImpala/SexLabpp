#pragma once

#include "InterType.h"
#include "Thread/NiNode/NiInstance.h"
#include "Thread/NiNode/Legacy/LegacyNiUpdate.h"
#include <vector>
#include <unordered_map>
#include <shared_mutex>

namespace Thread::Interactions
{
    struct InteractionData
    {
        RE::FormID positionActor{};
        RE::FormID partnerActor{};
        InterType type{ InterType::pStimulation };
        float velocity{ 0.0f };
        bool fromCollision{ true };  // true if detected via collision, false if fallback to tags
        float timeActive{ 0.0f };    // for hysteresis
        bool active{ false };
    };

    class InteractionDetector final : public Singleton<InteractionDetector>
    {
      public:
        // Register a thread's NiInstance for collision-based detection
        void RegisterNiInstance(RE::FormID threadFormID, NiNode::NiInstance* niInstance);
        void RegisterNiInstanceLegacy(RE::FormID threadFormID, LegacyNiNode::NiInstance* niInstance);
        
        // Unregister a thread
        void Unregister(RE::FormID threadFormID);
        
        // Get all interaction types between two actors (returns InterType values)
        [[nodiscard]] std::vector<int> GetInteractionTypes(RE::FormID threadFormID, 
            RE::Actor* position, RE::Actor* partner) const;
        
        // Check if specific interaction type exists
        [[nodiscard]] bool HasInteractionType(RE::FormID threadFormID, int interTypeValue,
            RE::Actor* position, RE::Actor* partner) const;
        
        // Get partner by interaction type (forward direction)
        [[nodiscard]] RE::Actor* GetPartnerByType(RE::FormID threadFormID,
            RE::Actor* position, int interTypeValue) const;
        
        // Get all partners by interaction type (forward direction)
        [[nodiscard]] std::vector<RE::Actor*> GetPartnersByType(RE::FormID threadFormID,
            RE::Actor* position, int interTypeValue) const;
        
        // Get partner by interaction type (reverse direction)
        [[nodiscard]] RE::Actor* GetPartnerByTypeRev(RE::FormID threadFormID,
            RE::Actor* partner, int interTypeValue) const;
        
        // Get all partners by interaction type (reverse direction)
        [[nodiscard]] std::vector<RE::Actor*> GetPartnersByTypeRev(RE::FormID threadFormID,
            RE::Actor* partner, int interTypeValue) const;
        
        // Get velocity of specific interaction
        [[nodiscard]] float GetActionVelocity(RE::FormID threadFormID,
            RE::Actor* position, RE::Actor* partner, int interTypeValue) const;
        
        // Get current interaction flags for an actor (28 bools as bitset)
        [[nodiscard]] std::array<bool, SUPPORTED_INTER_COUNT> GetCurrentInteractionFlags(
            RE::FormID threadFormID, RE::Actor* position) const;
        
        // Check if collision data is registered
        [[nodiscard]] bool IsRegistered(RE::FormID threadFormID) const;
        
        // Update detector state (called per frame for registered threads)
        void Update(float a_delta);
        
        // Use tag-based fallback when collision data unavailable
        void SetUseTagFallback(bool a_enable) { m_useTagFallback = a_enable; }
        [[nodiscard]] bool GetUseTagFallback() const { return m_useTagFallback; }

      private:
        // Internal lookup using ML (NiNode) system
        std::vector<int> GetInteractionTypesML(NiNode::NiInstance* niInstance,
            RE::Actor* position, RE::Actor* partner) const;
        
        // Internal lookup using legacy system
        std::vector<int> GetInteractionTypesLegacy(LegacyNiNode::NiInstance* niInstance,
            RE::Actor* position, RE::Actor* partner) const;
        
        // Tag-based fallback detection
        std::vector<int> GetInteractionTypesByTags(RE::Actor* position, 
            RE::Actor* partner, const Registry::Stage* activeStage) const;
        
        struct ThreadData
        {
            std::weak_ptr<NiNode::NiInstance> niInstance{};
            std::weak_ptr<LegacyNiNode::NiInstance> niInstanceLegacy{};
            bool useLegacy{ false };
        };
        
        mutable std::shared_mutex m_mutex{};
        std::unordered_map<RE::FormID, ThreadData> m_threadData{};
        bool m_useTagFallback{ true };
    };

}  // namespace Thread::Interactions

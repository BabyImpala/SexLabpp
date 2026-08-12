#pragma once

#include "LegacyNiPosition.h"
#include "LegacyNode.h"
#include "Registry/Define/Animation.h"

namespace Thread::LegacyNiNode
{
    struct NiInstance
    {
        friend class NiUpdate;

      public:
        NiInstance(const std::vector<RE::Actor*>& a_positions, const Registry::Scene* a_scene);
        ~NiInstance() = default;

        bool VisitPositions(std::function<bool(const NiPosition&)> a_visitor) const;

      private:
        void UpdateInteractions(float a_delta, bool a_drawCollision);
        void GetInteractionsMale(std::vector<NiPosition::Snapshot>& list, const NiPosition::Snapshot& it);
        void GetInteractionsFemale(std::vector<NiPosition::Snapshot>& list, const NiPosition::Snapshot& it);
        void GetInteractionsNeutral(std::vector<NiPosition::Snapshot>& list, const NiPosition::Snapshot& it);

        std::vector<NiPosition> positions;
        mutable std::mutex _m{};
    };

    class NiUpdate
    {
      public:
        static void OnFrameUpdate(float a_delta);

        static std::shared_ptr<NiInstance> Register(RE::FormID a_id, std::vector<RE::Actor*> a_positions, const Registry::Scene* a_scene) noexcept;
        static void Unregister(RE::FormID a_id) noexcept;

      private:
        static inline std::mutex _m{};
        static inline std::vector<std::pair<RE::FormID, std::shared_ptr<NiInstance>>> processes;
    };

}  // namespace Thread::LegacyNiNode

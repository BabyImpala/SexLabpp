#pragma once
#include "Thread/Interface/PrismaSceneMenu.h"

namespace Thread::PrismaUI
{
    class OffsetAdjustMenu
    {
      public:
        static void Init();
        static void Destroy();

        static void OnStageChanged();
        static void SetOffsetsDisplay(uint32_t actorId);

        static void OnActorSelected(const std::string& actorIdStr);
        static void OnSetOffset(const std::string& data);
        static void OnResetOffsets();
        static void OnSetAdjustStageOnly(const std::string& data);

      private:
        struct ActorSlot {
            RE::Actor* actor;
            uint32_t formId;
            size_t positionIndex;
        };

        inline static bool isOverlayVisible{ false };
        inline static std::vector<ActorSlot> s_slots;
        inline static std::optional<uint32_t> s_selectedActorId = std::nullopt;
   
        static std::string BuildInitJson();
        static const ActorSlot* FindSlot(uint32_t actorId);
    };

}  // namespace Thread::PrismaUI

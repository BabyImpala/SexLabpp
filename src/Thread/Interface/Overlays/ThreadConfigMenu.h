#pragma once
#include "Thread/Interface/PrismaSceneMenu.h"

namespace Thread::PrismaUI
{
    class ThreadConfigMenu
    {
      public:
        static void Init();
        static void Destroy();

        static void OnRandomScene();
        static void OnMoveScene();
        static void OnAutoPlaySet(const std::string& data);
        static void OnNextPermutation(const std::string& data);
        static void OnSetExpression(const std::string& data);
        static void OnSetVoice(const std::string& data);
        static void OnSetActorAlpha(const std::string& data);

      private:
        struct ActorSlot {
            RE::Actor* actor;
            uint32_t formId;
            bool isPlayer;
        };

        inline static bool isOverlayVisible{ false };
        inline static std::vector<ActorSlot> s_slots;

        static std::string BuildInitJson();
        static RE::Actor* FindActor(uint32_t formId);
    };

}  // namespace Thread::PrismaUI
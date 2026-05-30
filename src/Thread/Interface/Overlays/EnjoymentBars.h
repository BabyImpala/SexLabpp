#pragma once
#include "Thread/Interface/PrismaSceneMenu.h"

namespace Thread::PrismaUI
{
    class EnjoymentBars
    {
      public:
        static void Init();
        static void Destroy();

        static void UpdateSlider(RE::Actor* a_actor, float a_enjoyment, RE::BSFixedString a_interactions);
        static void UpdateHighlightedPartner(RE::Actor* a_partner);
        static void RegisterRaiseEnjAttempt(RE::Actor* a_actor, float a_nextTimeCycle);

        static void OnRaiseEnjAttemptResult(bool a_success);
        static void OnSelectPartner(const std::string& data);

      private:
        inline static bool isOverlayVisible{ false };

        static std::string BuildInitJson();
    };

}  // namespace Thread::PrismaUI

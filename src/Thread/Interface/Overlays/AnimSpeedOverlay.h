#pragma once
#include "Thread/Interface/PrismaSceneMenu.h"

namespace Thread::PrismaUI
{
    class AnimSpeedOverlay
    {
      public:
        static void Init();
        static void Destroy();

        static void SetAnimSpeedDisplay(float value);
        static void UpdateStageTimerDisplay(float duration, float timer);

        static void OnAnimSpeedChange(const std::string& data);

      private:
        inline static bool isOverlayVisible{ false };
        inline static float animPlaybackSpeed{ 1.0f };
    };

}  // namespace Thread::PrismaUI

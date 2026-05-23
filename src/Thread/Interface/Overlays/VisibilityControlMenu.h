#pragma once
#include "Thread/Interface/PrismaSceneMenu.h"

namespace Thread::PrismaUI
{
    class VisibilityControlMenu
    {
      public:
        static void Init();
        static void Destroy();

        static void PushOverlayState(int32_t index, bool state);

        static void OnMenuScaleChange(const std::string& data);
        static void OnOverlayToggle(const std::string& data);

      private:
        inline static bool isOverlayVisible{ false };

        static std::string BuildInitJson();
    };

}  // namespace Thread::PrismaUI
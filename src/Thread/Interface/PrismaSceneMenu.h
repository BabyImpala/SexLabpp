#pragma once
#include "PrismaUtil.h"

#include "Thread/Thread.h"
#include "Util/Script.h"

namespace Thread::PrismaUI
{
    class PrismaSceneMenu
    {
      public:
        static inline PrismaView psmView{ 0 };
        static inline RE::TESQuest* psm_linkedThread{ nullptr };
        static inline Script::ObjectPtr psm_threadScript{ nullptr };
        static inline Script::CallbackPtr psm_callbackPtr{};

        static PrismaView* GetView() { return &psmView; }

        static bool Register();
        static void Init(RE::TESQuest* a_qst);
        static void Destroy();

        static void ToggleFocus();

        static void CollapseAllPanels();
        static void DestroyAllOverlays();

      private:
        static inline constexpr std::string_view FILEPATH{ "SexLab\\PrismaSceneMenu.html" };
    };

}  // namespace Thread::PrismaUI

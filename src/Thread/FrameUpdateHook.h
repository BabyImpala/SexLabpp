#pragma once

namespace Thread
{
    class FrameUpdateHook
    {
      public:
        static void Install();

      private:
        static void OnFrameUpdate();
        static inline REL::Relocation<decltype(OnFrameUpdate)> _OriginalFunction;
    };
}

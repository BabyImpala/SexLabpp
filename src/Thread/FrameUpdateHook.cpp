#include "FrameUpdateHook.h"

#include "Thread.h"

namespace Thread
{
    void FrameUpdateHook::Install()
    {
        auto& trampoline = SKSE::GetTrampoline();

        const auto address = REL::VariantID(35565, 36564, 0x5BAB10).address();
        const auto offset = REL::VariantOffset(0x748, 0xC26, 0x7EE).offset();

        _OriginalFunction = trampoline.write_call<5>(address + offset, OnFrameUpdate);
        logger::info("Installed frame hook");
    }

    void FrameUpdateHook::OnFrameUpdate()
    {
        static REL::Relocation<float*> deltaTime{ REL::VariantID(523660, 410199, 0x30C3A08) };
        const auto frameDelta = *deltaTime;
        Instance::UpdateAnimations(frameDelta);
        _OriginalFunction();
    }
}

#include "AnimSpeedOverlay.h"

namespace Thread::PrismaUI
{
    void AnimSpeedOverlay::Init()
    {
        if (!IsViewValid(&PrismaSceneMenu::psmView)) return;
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "aso_initOverlay", std::to_string(animPlaybackSpeed).c_str());
        isOverlayVisible = true;
    };

    void AnimSpeedOverlay::Destroy()
    {
        if (!IsViewValid(&PrismaSceneMenu::psmView)) return;
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "aso_destroyOverlay", "");
        isOverlayVisible = false;
        animPlaybackSpeed = 1.0f;
    };

    // ── C++ to JS

    void AnimSpeedOverlay::SetAnimSpeedDisplay(float value)
    {
        if (!isOverlayVisible) return;
        if (IsViewValid(&PrismaSceneMenu::psmView)) {
            PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "aso_setAnimSpeedDisplay",
                std::to_string(value).c_str());
        }
    };

    void AnimSpeedOverlay::UpdateStageTimerDisplay(float duration, float timer)
    {
        if (!isOverlayVisible) return;
        if (IsViewValid(&PrismaSceneMenu::psmView)) {
            PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "aso_setStageTimerDisplay",
                (std::to_string(duration) + "^" + std::to_string(timer)).c_str());
        }
    };

    // ── JS TO C++

    void AnimSpeedOverlay::OnAnimSpeedChange(const std::string& data)
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return;

        float delta = std::stof(data);
        float newSpeed = std::clamp(animPlaybackSpeed + delta, 0.25f, 3.0f);
        animPlaybackSpeed = newSpeed;

        instance->SetAnimationPlaybackSpeed(animPlaybackSpeed);
        SetAnimSpeedDisplay(animPlaybackSpeed);
        
        Script::DispatchMethodCall(Script::GetScriptObject(PrismaSceneMenu::psm_linkedThread, "sslThreadModel"),
            "UpdateBaseSpeed", PrismaSceneMenu::psm_callbackPtr, static_cast<float>(animPlaybackSpeed));
    };

}  // namespace Thread::PrismaUI

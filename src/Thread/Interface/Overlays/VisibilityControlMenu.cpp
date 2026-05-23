#include "VisibilityControlMenu.h"
#include "Papyrus/SexLabUtil.h"

namespace Thread::PrismaUI
{
    void VisibilityControlMenu::Init()
    {
        if (!IsViewValid(&PrismaSceneMenu::psmView)) return;
        const std::string json = BuildInitJson();
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "vcm_initOverlay", json.c_str());
        isOverlayVisible = true;
    };

    void VisibilityControlMenu::Destroy()
    {
        if (!IsViewValid(&PrismaSceneMenu::psmView)) return;
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "vcm_destroyOverlay", "");
        isOverlayVisible = false;
    };

    // ── C++ TO JS

    void VisibilityControlMenu::PushOverlayState(int32_t index, bool state)
    {
        if (!isOverlayVisible) return;
        const std::string payload = std::to_string(index) + '^' + (state ? "true" : "false");
        if (IsViewValid(&PrismaSceneMenu::psmView)) {
            PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "vcm_setOverlayState", payload.c_str());
        }
    };

    // ── JS TO C++
    
    void VisibilityControlMenu::OnMenuScaleChange(const std::string& data)
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return;
        float adj = instance->GetThreadProperty<float>("MenuScaleMult");
        try { adj = std::stof(data); } catch (...) { }
        adj = std::clamp(adj, 0.5f, 2.5f);
        instance->SetThreadProperty<float>("MenuScaleMult", adj);
    };

    void VisibilityControlMenu::OnOverlayToggle(const std::string& data)
    {
        // payload: "idxVal^true"
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return;

        const auto sep = data.find('^');
        if (sep == std::string::npos) return;
        int32_t idxVal = -1;
        try { idxVal = std::stoi(data.substr(0, sep)); } catch (...) { return; }
        const bool state = data.substr(sep + 1) == "true";

        std::string propName;
        switch (idxVal) {
        case -1: propName = "GameHUD"; break;
        case 1: propName  = "OverlayAnimSpeed"; break;
        case 2: propName  = "OverlayEnjBars"; break;
        case 3: propName  = "OverlayOffsetAdjust"; break;
        case 4: propName  = "OverlaySceneSelector"; break;
        case 5: propName  = "OverlayThreadConfig"; break;
        default: return;
        }

        instance->SetThreadProperty<bool>(propName, state);

        if (idxVal == -1) {
            Papyrus::SexLabUtil::HideElementsGameHUD(nullptr, !state);
        } else {
            auto overlayIdx = static_cast<PrismaOverlayIndex>(idxVal);
            if (state) {
                OverlayInit(PrismaSceneMenu::psm_linkedThread, overlayIdx);
            } else {
                OverlayDestroy(overlayIdx);
            }
        }

        PushOverlayState(idxVal, state);
    };

    // ── HELPERS

    std::string VisibilityControlMenu::BuildInitJson()
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return "{}";
        
        auto getPropertyState = [&](const std::string& prop) {
            return instance->GetThreadProperty<bool>(prop) ? "true" : "false";
        };

        float scaleAdj = instance->GetThreadProperty<float>("MenuScaleMult");
        scaleAdj = std::clamp(scaleAdj, 0.5f, 2.5f);

        return std::format(
            R"({{"scaleAdj":{},"states":{{"-1":{},"1":{},"2":{},"3":{},"4":{},"5":{}}}}})",
            scaleAdj,
            getPropertyState("GameHUD"),
            getPropertyState("OverlayAnimSpeed"),
            getPropertyState("OverlayEnjBars"),
            getPropertyState("OverlayOffsetAdjust"),
            getPropertyState("OverlaySceneSelector"),
            getPropertyState("OverlayThreadConfig")
        );
    };

}  // namespace Thread::PrismaUI
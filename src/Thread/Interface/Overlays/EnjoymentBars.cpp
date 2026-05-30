#include "EnjoymentBars.h"

namespace Thread::PrismaUI
{
    void EnjoymentBars::Init()
    {
        if (!IsViewValid(&PrismaSceneMenu::psmView)) return;
        const std::string json = BuildInitJson();
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "ebo_initOverlay", json.c_str());
        isOverlayVisible = true;
    };

    void EnjoymentBars::Destroy()
    {
        if (!IsViewValid(&PrismaSceneMenu::psmView)) return;
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "ebo_destroyOverlay", "");
        isOverlayVisible = false;
    };

    // ── C++ to JS

    void EnjoymentBars::UpdateSlider(RE::Actor* a_actor, float a_enjoyment, RE::BSFixedString a_interactions)
    {
        if (!isOverlayVisible) return;
        std::string payload = std::to_string(a_actor->GetFormID()) + "^" + std::to_string(a_enjoyment) + "^" + a_interactions.c_str();
        if (IsViewValid(&PrismaSceneMenu::psmView)) {
            PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "ebo_UpdateSlider", payload.c_str());
        }
    };

    void EnjoymentBars::UpdateHighlightedPartner(RE::Actor* a_partner)
    {
        if (!isOverlayVisible) return;
        std::string partnerID = std::to_string(a_partner->GetFormID());
        if (IsViewValid(&PrismaSceneMenu::psmView)) {
            PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "ebo_UpdateHighlightedPartner", partnerID.c_str());
        }
    };

    void EnjoymentBars::RegisterRaiseEnjAttempt(RE::Actor* a_actor, float a_nextTimeCycle)
    {
        if (!isOverlayVisible) return;
        std::string payload = std::to_string(a_actor->GetFormID()) + "^" + std::to_string(a_nextTimeCycle);
        if (IsViewValid(&PrismaSceneMenu::psmView)) {
            PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "ebo_RaiseEnjAttempt", payload.c_str());
        }
    };

    // ── JS TO C++

    void EnjoymentBars::OnRaiseEnjAttemptResult(bool a_success)
    {
        if (!PrismaSceneMenu::psm_threadScript) return;
        Script::DispatchMethodCall(PrismaSceneMenu::psm_threadScript, "OnRaiseEnjAttemptResult",
            PrismaSceneMenu::psm_callbackPtr, a_success ? 1 : 0);
    };

    void EnjoymentBars::OnSelectPartner(const std::string& data)
    {
        uint32_t formId = 0;
        try { formId = static_cast<uint32_t>(std::stoul(data)); } catch (...) { return; }
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(formId);
        if (!actor || actor->IsPlayerRef()) return;

        if (!PrismaSceneMenu::psm_threadScript) return;
        Script::DispatchMethodCall(PrismaSceneMenu::psm_threadScript, "SelectTargetPartner",
            PrismaSceneMenu::psm_callbackPtr, std::move(actor));
    };

    // ── HELPERS

    std::string EnjoymentBars::BuildInitJson()
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return "{}";

        const auto actors = instance->GetActors();
        std::string json = "{\"ebo_actorsInfo\":[";
        for (size_t i = 0; i < actors.size(); ++i) {
            if (i > 0)
                json += ',';
            json += "{\"id\":";     json += std::to_string(actors[i]->GetFormID());
            json += ",\"name\":\""; json += JsonEscape(actors[i]->GetName());
            json += "\"}";
        }
        json += "]}";

        return json;
    };

}  // namespace Thread::PrismaUI

#include "PrismaSceneMenu.h"

#include "Overlays/AnimSpeedOverlay.h"
#include "Overlays/EnjoymentBars.h"
#include "Overlays/OffsetAdjustMenu.h"
#include "Overlays/SceneSelectorMenu.h"
#include "Overlays/ThreadConfigMenu.h"
#include "Overlays/VisibilityControlMenu.h"

namespace Thread::PrismaUI
{
    bool PrismaSceneMenu::Register()
    {
        const bool ok = CreatePrismaView(FILEPATH, &psmView, []() {

            // ── AnimSpeedOverlay
            PrismaAPI->RegisterJSListener(psmView, "aso_OnAnimSpeedChange", [](const char* data) {
                if (data && *data) AnimSpeedOverlay::OnAnimSpeedChange(data);
            });

            // ── EnjoymentBars
            PrismaAPI->RegisterJSListener(psmView, "ebo_OnTimedAttempt", []([[maybe_unused]] const char*) {
                EnjoymentBars::OnRaiseEnjAttemptResult(true);
            });
            PrismaAPI->RegisterJSListener(psmView, "ebo_OnMissedAttempt", []([[maybe_unused]] const char*) {
                EnjoymentBars::OnRaiseEnjAttemptResult(false);
            });

            // ── OffsetAdjustMenu
            PrismaAPI->RegisterJSListener(psmView, "oam_OnActorSelected", [](const char* data) {
                if (data && *data) OffsetAdjustMenu::OnActorSelected(data);
            });
            PrismaAPI->RegisterJSListener(psmView, "oam_OnSetOffset", [](const char* data) {
                if (data && *data) OffsetAdjustMenu::OnSetOffset(data);
            });
            PrismaAPI->RegisterJSListener(psmView, "oam_OnResetOffsets", []([[maybe_unused]] const char*) {
                OffsetAdjustMenu::OnResetOffsets();
            });
            PrismaAPI->RegisterJSListener(psmView, "oam_OnSetAdjustStageOnly", [](const char* data) {
                if (data && *data) OffsetAdjustMenu::OnSetAdjustStageOnly(data);
            });

            // ── SceneSelectorMenu
            PrismaAPI->RegisterJSListener(psmView, "ssm_OnSceneSelected", [](const char* data) {
                if (data && *data) SceneSelectorMenu::OnSceneSelected(data);
            });
            PrismaAPI->RegisterJSListener(psmView, "ssm_OnSceneResetBySearch", [](const char* data) {
                if (data && *data) SceneSelectorMenu::OnSceneResetBySearch(data);
            });
            PrismaAPI->RegisterJSListener(psmView, "ssm_OnAnnotationEdited", [](const char* data) {
                if (data && *data) SceneSelectorMenu::OnAnnotationEdited(data);
            });

            // ── ThreadConfigMenu
            PrismaAPI->RegisterJSListener(psmView, "tcm_OnRandomScene", []([[maybe_unused]] const char*) {
                ThreadConfigMenu::OnRandomScene("");
            });
            PrismaAPI->RegisterJSListener(psmView, "tcm_OnMoveScene", []([[maybe_unused]] const char*) {
                ThreadConfigMenu::OnMoveScene("");
            });
            PrismaAPI->RegisterJSListener(psmView, "tcm_OnAutoPlaySet", [](const char* data) {
                if (data && *data) ThreadConfigMenu::OnAutoPlaySet(data);
            });
            PrismaAPI->RegisterJSListener(psmView, "tcm_OnNextPermutation", [](const char* data) {
                if (data && *data) ThreadConfigMenu::OnNextPermutation(data);
            });
            PrismaAPI->RegisterJSListener(psmView, "tcm_OnSetExpression", [](const char* data) {
                if (data && *data) ThreadConfigMenu::OnSetExpression(data);
            });
            PrismaAPI->RegisterJSListener(psmView, "tcm_OnSetVoice", [](const char* data) {
                if (data && *data) ThreadConfigMenu::OnSetVoice(data);
            });
            PrismaAPI->RegisterJSListener(psmView, "tcm_OnSetActorAlpha", [](const char* data) {
                if (data && *data) ThreadConfigMenu::OnSetActorAlpha(data);
            });

            // ── VisibilityControlMenu
            PrismaAPI->RegisterJSListener(psmView, "vcm_OnMenuScaleChange", [](const char* data) {
                if (data && *data) VisibilityControlMenu::OnMenuScaleChange(data);
            });
            PrismaAPI->RegisterJSListener(psmView, "vcm_OnOverlayToggle", [](const char* data) {
                if (data && *data) VisibilityControlMenu::OnOverlayToggle(data);
            });

            logger::info("PrismaSceneMenu::Register >> all listeners registered for shared view");
        });
        return ok;
    };

    void PrismaSceneMenu::Init(RE::TESQuest* a_qst)
    {
        if (!IsViewValid(&psmView) || !a_qst) {
            logger::warn("PrismaSceneMenu::Init >> initialization attempt failed, abandoning!");
            return;
        }
        psm_linkedThread = a_qst;
        psm_threadScript = Script::GetScriptObject(psm_linkedThread, "sslThreadController");
        auto* instance = Instance::GetInstance(psm_linkedThread);
        if (!instance) {
            logger::warn("PrismaSceneMenu::Init >> attched thread is null, abandoning!");
            return;
        }
        ShowView(&psmView);
        logger::info("PrismaSceneMenu::Init >> shared view initiated");
    };

    void PrismaSceneMenu::Destroy()
    {
        if (!IsViewValid(&psmView) || !psm_linkedThread) { return; }
        DestroyAllOverlays();
        HideView(&psmView);
        psm_linkedThread = nullptr;
        psm_threadScript = nullptr;
        logger::info("PrismaSceneMenu::Destroy >> shared view destroyed");
    };

    void PrismaSceneMenu::ToggleFocus()
    {
        if (!IsViewValid(&psmView)) return;
        CollapseAllPanels();
        if (PrismaAPI->HasFocus(psmView)) {
            PrismaAPI->Unfocus(psmView);
        } else {
            PrismaAPI->Focus(psmView);
        }
    };

    void PrismaSceneMenu::CollapseAllPanels()
    {
        if (!IsViewValid(&psmView)) return;
        PrismaAPI->InteropCall(psmView, "slp_collapseAllPanels", "");
    };

    void PrismaSceneMenu::DestroyAllOverlays()
    {
        AnimSpeedOverlay::Destroy();
        EnjoymentBars::Destroy();
        OffsetAdjustMenu::Destroy();
        SceneSelectorMenu::Destroy();
        ThreadConfigMenu::Destroy();
        VisibilityControlMenu::Destroy();
    };

}  // namespace Thread::PrismaUI
